//#include "Enclave_t.h"

#include <mutex>
#include <memory>
#include <map>

#include <DecentApi/Common/Common.h>
#include <DecentApi/Common/make_unique.h>
#include <DecentApi/Common/Ra/ClientX509Cert.h>
#include <DecentApi/Common/Ra/TlsConfigWithName.h>
#include <DecentApi/Common/Ra/KeyContainer.h>
#include <DecentApi/Common/Ra/CertContainer.h>
#include <DecentApi/Common/Net/TlsCommLayer.h>
#include <DecentApi/Common/Tools/JsonTools.h>
#include <DecentApi/Common/MbedTls/Drbg.h>
#include <DecentApi/Common/MbedTls/EcKey.h>
#include <DecentApi/Common/MbedTls/X509Req.h>
#include <DecentApi/CommonEnclave/Net/EnclaveCntTranslator.h>
#include <DecentApi/DecentAppEnclave/AppStatesSingleton.h>

#include <rapidjson/document.h>
#include <cppcodec/base64_default_rfc4648.hpp>

#include "../Common/AppNames.h"
#include "../Common/RideSharingFuncNums.h"
#include "../Common/RideSharingMessages.h"

#include "../Common_Enc/OperatorPayment.h"

using namespace RideShare;
using namespace Decent::Ra;
using namespace Decent::Net;

namespace
{
	static AppStates& gs_state = GetAppStateSingleton();
	
	struct PasProfileItem
	{
		std::string m_name;
		std::string m_phone;
		std::string m_pay;

		PasProfileItem(const std::string& name, const std::string& phone, const std::string& pay) :
			m_name(name),
			m_phone(phone),
			m_pay(pay)
		{}

		PasProfileItem(PasProfileItem&& rhs) :
			m_name(std::move(rhs.m_name)),
			m_phone(std::move(rhs.m_phone)),
			m_pay(std::move(rhs.m_pay))
		{}
	};

	std::map<std::string, PasProfileItem> gs_pasProfiles;
	const std::map<std::string, PasProfileItem>& gsk_pasProfiles = gs_pasProfiles;
	std::mutex gs_pasProfilesMutex;

	template<typename MsgType>
	static std::unique_ptr<MsgType> ParseMsg(const std::string& msgStr)
	{
		rapidjson::Document json;
		Decent::Tools::ParseStr2Json(json, msgStr);
		return Decent::Tools::make_unique<MsgType>(json);
	}

	static bool VerifyContactInfo(const ComMsg::PasContact& contact)
	{
		return contact.GetName().size() != 0 && contact.GetPhone().size() != 0;
	}
}

static void ProcessPasRegisterReq(void* const connection, Decent::Net::TlsCommLayer& tls)
{
	using namespace Decent::MbedTlsObj;

	LOGI("Processing Passenger Register Request...");

	EnclaveCntTranslator cnt(connection);

	std::string msgBuf = tls.RecvContainer<std::string>(cnt);
	std::unique_ptr<ComMsg::PasReg> pasRegInfo = ParseMsg<ComMsg::PasReg>(msgBuf);

	if (!VerifyContactInfo(pasRegInfo->GetContact()))
	{
		return;
	}

	X509Req certReq = pasRegInfo->GetCsr();
	certReq.VerifySignature();

	EcPublicKey<EcKeyType::SECP256R1> clientKey(EcPublicKey<EcKeyType::SECP256R1>(certReq.GetPublicKey()));
	std::string pasId = clientKey.GetPublicPem();

	{
		std::unique_lock<std::mutex> pasProfilesLock(gs_pasProfilesMutex);
		if (gsk_pasProfiles.find(pasId) != gsk_pasProfiles.cend())
		{
			LOGI("Client profile already exist.");
			return;
		}
	}
	std::shared_ptr<const AppX509Cert> cert = gs_state.GetAppCertContainer().GetAppCert();

	if (!cert)
	{
		LOGW("Decent App private key and certificate hasn't been initialized yet.");
		return;
	}

	EcKeyPair<EcKeyType::SECP256R1> prvKey = EcKeyPair<EcKeyType::SECP256R1>(*(gs_state.GetKeyContainer().GetSignKeyPair()));

	ClientX509CertWriter clientCert(clientKey, *cert, prvKey, "Decent_RideShare_Passenger",
		cppcodec::base64_rfc4648::encode(pasRegInfo->GetContact().CalcHash()));

	{
		std::unique_lock<std::mutex> pasProfilesLock(gs_pasProfilesMutex);

		gs_pasProfiles.insert(std::make_pair(pasId,
			PasProfileItem(pasRegInfo->GetContact().GetName(), pasRegInfo->GetContact().GetPhone(), pasRegInfo->GetPayment())));

		LOGI("Client profile added. Profile store size: %llu.", gsk_pasProfiles.size());
	}

	Drbg drbg;
	tls.SendContainer(cnt, clientCert.GeneratePemChain(drbg));

}

extern "C" int ecall_ride_share_pm_from_pas(void* const connection)
{
	if (!OperatorPayment::IsPaymentInfoValid())
	{
		return false;
	}

	using namespace EncFunc::PassengerMgm;

	LOGI("Processing message from passenger...");

	EnclaveCntTranslator cnt(connection);

	try
	{
		std::shared_ptr<TlsConfigWithName> tlsCfg = std::make_shared<TlsConfigWithName>(gs_state, TlsConfigWithName::Mode::ServerNoVerifyPeer, "NaN", nullptr);
		TlsCommLayer tls(cnt, tlsCfg, false, nullptr);

		NumType funcNum;
		tls.RecvStruct(cnt, funcNum);

		switch (funcNum)
		{
		case k_userReg:
			ProcessPasRegisterReq(connection, tls);
			break;
		default:
			break;
		}
	}
	catch (const std::exception& e)
	{
		PRINT_W("Failed to processing message from passenger. Caught exception: %s", e.what());
	}

	return false;
}

static void LogQuery(void* const connection, Decent::Net::TlsCommLayer& tls)
{
	LOGI("Logging Passenger Query...");

	EnclaveCntTranslator cnt(connection);

	std::string msgBuf = tls.RecvContainer<std::string>(cnt);
	std::unique_ptr<ComMsg::PasQueryLog> queryLog = ParseMsg<ComMsg::PasQueryLog>(msgBuf);

	const ComMsg::GetQuote& getQuote = queryLog->GetGetQuote();

	LOGI("Logged Passenger Query:");
	LOGI("Passenger ID:\n%s", queryLog->GetUserId().c_str());
	LOGI("Origin:      (%f, %f)", getQuote.GetOri().GetX(), getQuote.GetOri().GetY());
	LOGI("Destination: (%f, %f)", getQuote.GetDest().GetX(), getQuote.GetDest().GetY());

}

extern "C" int ecall_ride_share_pm_from_trip_planner(void* const connection)
{
	if (!OperatorPayment::IsPaymentInfoValid())
	{
		return false;
	}

	using namespace EncFunc::PassengerMgm;

	LOGI("Processing message from Trip Planner...");

	EnclaveCntTranslator cnt(connection);

	try
	{
		std::shared_ptr<TlsConfigWithName> tlsCfg = std::make_shared<TlsConfigWithName>(gs_state, TlsConfigWithName::Mode::ServerVerifyPeer, AppNames::sk_tripPlanner, nullptr);
		TlsCommLayer tls(cnt, tlsCfg, true, nullptr);

		NumType funcNum;
		tls.RecvStruct(cnt, funcNum);

		switch (funcNum)
		{
		case k_logQuery:
			LogQuery(connection, tls);
			break;
		default:
			break;
		}
	}
	catch (const std::exception& e)
	{
		PRINT_W("Failed to processing message from Trip Planner. Caught exception: %s", e.what());
	}

	return false;
}

static void RequestPaymentInfo(void* const connection, Decent::Net::TlsCommLayer& tls)
{
	LOGI("Processing payment info request...");

	EnclaveCntTranslator cnt(connection);

	std::string pasId = tls.RecvContainer<std::string>(cnt);

	std::string pasPayInfo;
	std::string selfPayInfo = OperatorPayment::GetPaymentInfo();
	{
		std::unique_lock<std::mutex> pasProfilesLock(gs_pasProfilesMutex);
		auto it = gsk_pasProfiles.find(pasId);
		if (it == gsk_pasProfiles.cend())
		{
			return;
		}
		pasPayInfo = it->second.m_pay;
	}

	LOGI("Passenger profile is found. Sending payment info...");

	ComMsg::RequestedPayment payInfo(std::move(pasPayInfo), std::move(selfPayInfo));

	tls.SendContainer(cnt, payInfo.ToString());
}

extern "C" int ecall_ride_share_pm_from_payment(void* const connection)
{
	if (!OperatorPayment::IsPaymentInfoValid())
	{
		return false;
	}

	using namespace EncFunc::PassengerMgm;

	LOGI("Processing message from Payment Services...");

	EnclaveCntTranslator cnt(connection);

	try
	{
		std::shared_ptr<TlsConfigWithName> payTlsCfg = std::make_shared<TlsConfigWithName>(gs_state, TlsConfigWithName::Mode::ServerVerifyPeer, AppNames::sk_payment, nullptr);
		TlsCommLayer tls(cnt, payTlsCfg, true, nullptr);

		NumType funcNum;
		tls.RecvStruct(cnt, funcNum);

		switch (funcNum)
		{
		case k_getPayInfo:
			RequestPaymentInfo(connection, tls);
			break;
		default:
			break;
		}
	}
	catch (const std::exception& e)
	{
		PRINT_W("Failed to processing message from Payment Services. Caught exception: %s", e.what());
	}
	

	return false;
}
