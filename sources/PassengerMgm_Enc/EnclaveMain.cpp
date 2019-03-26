//#include "Enclave_t.h"

#include <mutex>
#include <memory>
#include <map>

#include <DecentApi/Common/Common.h>
#include <DecentApi/Common/make_unique.h>
#include <DecentApi/Common/Ra/ClientX509.h>
#include <DecentApi/Common/Ra/TlsConfigWithName.h>
#include <DecentApi/Common/Ra/KeyContainer.h>
#include <DecentApi/Common/Ra/CertContainer.h>
#include <DecentApi/Common/Net/TlsCommLayer.h>
#include <DecentApi/Common/Tools/JsonTools.h>
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
	LOGI("Processing Passenger Register Request...");

	std::string msgBuf;

	tls.ReceiveMsg(connection, msgBuf);
	std::unique_ptr<ComMsg::PasReg> pasRegInfo = ParseMsg<ComMsg::PasReg>(msgBuf);

	if (!VerifyContactInfo(pasRegInfo->GetContact()))
	{
		return;
	}

	X509Req certReq = pasRegInfo->GetCsr();
	if (!certReq ||
		!certReq.VerifySignature() ||
		!certReq.GetEcPublicKey())
	{
		return;
	}

	const std::string pasId = certReq.GetEcPublicKey().ToPubPemString();

	{
		std::unique_lock<std::mutex> pasProfilesLock(gs_pasProfilesMutex);
		if (gsk_pasProfiles.find(pasId) != gsk_pasProfiles.cend())
		{
			LOGI("Client profile already exist.");
			return;
		}
	}

	std::shared_ptr<const Decent::MbedTlsObj::ECKeyPair> prvKey = gs_state.GetKeyContainer().GetSignKeyPair();
	std::shared_ptr<const AppX509> cert = gs_state.GetAppCertContainer().GetAppCert();

	if (!prvKey || !*prvKey ||
		!cert || !*cert)
	{
		return;
	}

	ClientX509 clientCert(certReq.GetEcPublicKey(), *cert, *prvKey, "Decent_RideShare_Passenger", 
		cppcodec::base64_rfc4648::encode(pasRegInfo->GetContact().CalcHash()));

	{
		std::unique_lock<std::mutex> pasProfilesLock(gs_pasProfilesMutex);

		gs_pasProfiles.insert(std::make_pair(pasId,
			PasProfileItem(pasRegInfo->GetContact().GetName(), pasRegInfo->GetContact().GetPhone(), pasRegInfo->GetPayment())));

		LOGI("Client profile added. Profile store size: %llu.", gsk_pasProfiles.size());
	}

	tls.SendMsg(connection, clientCert.ToPemString());

}

extern "C" int ecall_ride_share_pm_from_pas(void* const connection)
{
	if (!OperatorPayment::IsPaymentInfoValid())
	{
		return false;
	}

	using namespace EncFunc::PassengerMgm;

	LOGI("Processing message from passenger...");

	try
	{
		std::shared_ptr<TlsConfigWithName> tlsCfg = std::make_shared<TlsConfigWithName>(gs_state, TlsConfig::Mode::ServerNoVerifyPeer, "NaN");
		TlsCommLayer tls(connection, tlsCfg, false);

		NumType funcNum;
		tls.ReceiveStruct(connection, funcNum);

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

	std::string msgBuf;

	tls.ReceiveMsg(connection, msgBuf);
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

	try
	{
		std::shared_ptr<TlsConfigWithName> tlsCfg = std::make_shared<TlsConfigWithName>(gs_state, TlsConfig::Mode::ServerVerifyPeer, AppNames::sk_tripPlanner);
		TlsCommLayer tls(connection, tlsCfg, true);

		NumType funcNum;
		tls.ReceiveStruct(connection, funcNum);

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

	std::string pasId;
	tls.ReceiveMsg(connection, pasId);

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

	tls.SendMsg(connection, payInfo.ToString());
}

extern "C" int ecall_ride_share_pm_from_payment(void* const connection)
{
	if (!OperatorPayment::IsPaymentInfoValid())
	{
		return false;
	}

	using namespace EncFunc::PassengerMgm;

	LOGI("Processing message from Payment Services...");

	try
	{
		std::shared_ptr<TlsConfigWithName> payTlsCfg = std::make_shared<TlsConfigWithName>(gs_state, TlsConfig::Mode::ServerVerifyPeer, AppNames::sk_payment);
		TlsCommLayer tls(connection, payTlsCfg, true);

		NumType funcNum;
		tls.ReceiveStruct(connection, funcNum);

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
