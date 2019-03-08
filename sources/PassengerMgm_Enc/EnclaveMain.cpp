//#include "Enclave_t.h"

#include <mutex>
#include <memory>

#include <DecentApi/Common/Common.h>
#include <DecentApi/Common/make_unique.h>
#include <DecentApi/Common/Ra/TlsConfig.h>
#include <DecentApi/Common/Ra/KeyContainer.h>
#include <DecentApi/Common/Ra/CertContainer.h>
#include <DecentApi/Common/Net/TlsCommLayer.h>
#include <DecentApi/Common/Tools/JsonTools.h>
#include <DecentApi/DecentAppEnclave/AppStatesSingleton.h>

#include <rapidjson/document.h>
#include <cppcodec/base64_default_rfc4648.hpp>

#include "../Common/Crypto.h"
#include "../Common/TlsConfig.h"
#include "../Common/AppNames.h"
#include "../Common/RideSharingFuncNums.h"
#include "../Common/RideSharingMessages.h"

#include "../Common_Enc/OperatorPayment.h"

using namespace RideShare;
using namespace Decent::Ra;

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
		try
		{
			rapidjson::Document json;
			return Decent::Tools::ParseStr2Json(json, msgStr) ? 
				Decent::Tools::make_unique<MsgType>(json) :
				nullptr;
		}
		catch (const std::exception&)
		{
			return nullptr;
		}
	}
}

static bool VerifyContactInfo(const ComMsg::PasContact& contact)
{
	return contact.GetName().size() != 0 && contact.GetPhone().size() != 0;
}

static void ProcessPasRegisterReq(void* const connection, Decent::Net::TlsCommLayer& tls)
{
	LOGI("Processing Passenger Register Request...");

	std::string msgBuf;

	std::unique_ptr<ComMsg::PasReg> pasRegInfo;
	tls.ReceiveMsg(connection, msgBuf);
	if (!(pasRegInfo = ParseMsg<ComMsg::PasReg>(msgBuf)))
	{
		return;
	}

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

	ClientX509 clientCert(certReq.GetEcPublicKey(), *cert, *prvKey, "Decent_RideShare_Passenger_" + pasRegInfo->GetContact().GetName(), 
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
		std::shared_ptr<Decent::Ra::TlsConfig> pasTlsCfg = std::make_shared<Decent::Ra::TlsConfig>("NaN", gs_state, true);
		Decent::Net::TlsCommLayer pasTls(connection, pasTlsCfg, false);

		NumType funcNum;
		pasTls.ReceiveStruct(connection, funcNum);

		LOGI("Request Function: %d.", funcNum);

		switch (funcNum)
		{
		case k_userReg:
			ProcessPasRegisterReq(connection, pasTls);
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

	std::unique_ptr<ComMsg::PasQueryLog> queryLog;
	tls.ReceiveMsg(connection, msgBuf);
	if (!(queryLog = ParseMsg<ComMsg::PasQueryLog>(msgBuf)))
	{
		return;
	}

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
		std::shared_ptr<Decent::Ra::TlsConfig> tpTlsCfg = std::make_shared<Decent::Ra::TlsConfig>(AppNames::sk_tripPlanner, gs_state, true);
		Decent::Net::TlsCommLayer tpTls(connection, tpTlsCfg, true);

		NumType funcNum;
		tpTls.ReceiveStruct(connection, funcNum);

		LOGI("Request Function: %d.", funcNum);

		switch (funcNum)
		{
		case k_logQuery:
			LogQuery(connection, tpTls);
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
		std::shared_ptr<Decent::Ra::TlsConfig> payTlsCfg = std::make_shared<Decent::Ra::TlsConfig>(AppNames::sk_payment, gs_state, true);
		Decent::Net::TlsCommLayer payTls(connection, payTlsCfg, true);

		NumType funcNum;
		payTls.ReceiveStruct(connection, funcNum);

		LOGI("Request Function: %d.", funcNum);

		switch (funcNum)
		{
		case k_getPayInfo:
			RequestPaymentInfo(connection, payTls);
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
