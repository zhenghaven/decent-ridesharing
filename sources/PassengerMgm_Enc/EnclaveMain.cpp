//#include "Enclave_t.h"

#include <mutex>
#include <memory>

#include <DecentApi/Common/Common.h>
#include <DecentApi/Common/make_unique.h>
#include <DecentApi/Common/Ra/TlsConfig.h>
#include <DecentApi/Common/Ra/States.h>
#include <DecentApi/Common/Ra/KeyContainer.h>
#include <DecentApi/Common/Ra/CertContainer.h>
#include <DecentApi/Common/Net/TlsCommLayer.h>
#include <DecentApi/Common/Tools/JsonTools.h>

#include <rapidjson/document.h>

#include "../Common/Crypto.h"
#include "../Common/TlsConfig.h"
#include "../Common/AppNames.h"
#include "../Common/RideSharingFuncNums.h"
#include "../Common/RideSharingMessages.h"

using namespace RideShare;
using namespace Decent::Ra;

namespace
{
	static States& gs_state = States::Get();
	
	std::string gs_selfPaymentInfo = "Passenger Payment Info Test";
	
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
		//LOGW("Parsing Msg (size = %llu): \n%s\n", msgStr.size(), msgStr.c_str());
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

static void ProcessPasRegisterReq(void* const connection, Decent::Net::TlsCommLayer& tls)
{
	LOGI("Processing Passenger Register Request...");

	std::string msgBuf;

	std::unique_ptr<ComMsg::PasReg> pasRegInfo;
	if (!tls.ReceiveMsg(connection, msgBuf) ||
		!(pasRegInfo = ParseMsg<ComMsg::PasReg>(msgBuf)))
	{
		return;
	}

	const std::string& csrPem = pasRegInfo->GetCsr();
	X509Req certReq(csrPem);

	if (!certReq)
	{
		return;
	}

	const std::string pasPubPemStr = certReq.GetEcPublicKey().ToPubPemString();

	{
		std::unique_lock<std::mutex> pasProfilesLock(gs_pasProfilesMutex);
		if (gsk_pasProfiles.find(pasPubPemStr) != gsk_pasProfiles.cend())
		{
			LOGI("Client profile already exist.");
			return;
		}
	}

	std::shared_ptr<const Decent::MbedTlsObj::ECKeyPair> prvKey = gs_state.GetKeyContainer().GetSignKeyPair();
	std::shared_ptr<const AppX509> cert = std::dynamic_pointer_cast<const AppX509>(gs_state.GetCertContainer().GetCert());

	if (!certReq.VerifySignature() ||
		!certReq.GetEcPublicKey() ||
		!prvKey || !*prvKey ||
		!cert || !*cert)
	{
		return;
	}

	ClientX509 clientCert(certReq.GetEcPublicKey(), *cert, *prvKey, "RideSharingClient");

	{
		std::unique_lock<std::mutex> pasProfilesLock(gs_pasProfilesMutex);

		gs_pasProfiles.insert(std::make_pair(pasPubPemStr,
			PasProfileItem(pasRegInfo->GetContact().GetName(), pasRegInfo->GetContact().GetPhone(), pasRegInfo->GetPayment())));

		LOGI("Client profile added. Profile store size: %llu.", gsk_pasProfiles.size());
	}

	tls.SendMsg(connection, clientCert.ToPemString());

}

extern "C" int ecall_ride_share_pm_from_pas(void* const connection)
{
	using namespace EncFunc::PassengerMgm;

	LOGI("Processing message from passenger...");

	std::shared_ptr<Decent::Ra::TlsConfig> pasTlsCfg = std::make_shared<Decent::Ra::TlsConfig>("NaN", gs_state, true);
	Decent::Net::TlsCommLayer pasTls(connection, pasTlsCfg, false);

	std::string msgBuf;
	if (!pasTls.ReceiveMsg(connection, msgBuf) ||
		msgBuf.size() != sizeof(NumType))
	{
		LOGI("Recv size: %llu", msgBuf.size());
		return false;
	}

	LOGI("TLS Handshake Successful!");

	const NumType funcNum = EncFunc::GetNum<EncFunc::PassengerMgm::NumType>(msgBuf);

	LOGI("Request Function: %d.", funcNum);

	switch (funcNum)
	{
	case k_userReg:
		ProcessPasRegisterReq(connection, pasTls);
		break;
	default:
		break;
	}

	return false;
}

static void LogQuery(void* const connection, Decent::Net::TlsCommLayer& tls)
{
	LOGI("Logging Passenger Query...");

	std::string msgBuf;

	std::unique_ptr<ComMsg::PasQueryLog> queryLog;
	if (!tls.ReceiveMsg(connection, msgBuf) ||
		!(queryLog = ParseMsg<ComMsg::PasQueryLog>(msgBuf)))
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
	using namespace EncFunc::PassengerMgm;

	LOGI("Processing message from Trip Planner...");

	std::shared_ptr<Decent::Ra::TlsConfig> tpTlsCfg = std::make_shared<Decent::Ra::TlsConfig>(AppNames::sk_tripPlanner, gs_state, true);
	Decent::Net::TlsCommLayer tpTls(connection, tpTlsCfg, true);

	std::string msgBuf;
	if (!tpTls.ReceiveMsg(connection, msgBuf) ||
		msgBuf.size() != sizeof(NumType))
	{
		LOGW("Recv size: %llu", msgBuf.size());
		return false;
	}

	LOGI("TLS Handshake Successful!");

	const NumType funcNum = EncFunc::GetNum<NumType>(msgBuf);

	LOGI("Request Function: %d.", funcNum);

	switch (funcNum)
	{
	case k_logQuery:
		LogQuery(connection, tpTls);
		break;
	default:
		break;
	}

	return false;
}

static void RequestPaymentInfo(void* const connection, Decent::Net::TlsCommLayer& tls)
{
	LOGI("Processing payment info request...");

	std::string msgBuf;
	if (!tls.ReceiveMsg(connection, msgBuf))
	{
		return;
	}

	LOGI("Looking for payment info of passenger: %s", msgBuf.c_str());

	std::string pasPayInfo;
	std::string selfPayInfo = gs_selfPaymentInfo;
	{
		std::unique_lock<std::mutex> pasProfilesLock(gs_pasProfilesMutex);
		auto it = gsk_pasProfiles.find(msgBuf);
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
	using namespace EncFunc::PassengerMgm;

	LOGI("Processing message from payment services...");

	std::shared_ptr<Decent::Ra::TlsConfig> payTlsCfg = std::make_shared<Decent::Ra::TlsConfig>(AppNames::sk_payment, gs_state, true);
	Decent::Net::TlsCommLayer payTls(connection, payTlsCfg, true);

	std::string msgBuf;
	if (!payTls.ReceiveMsg(connection, msgBuf) ||
		msgBuf.size() != sizeof(NumType))
	{
		LOGW("Recv size: %llu", msgBuf.size());
		return false;
	}

	LOGI("TLS Handshake Successful!");

	const NumType funcNum = EncFunc::GetNum<NumType>(msgBuf);

	LOGI("Request Function: %d.", funcNum);

	switch (funcNum)
	{
	case k_getPayInfo:
		RequestPaymentInfo(connection, payTls);
		break;
	default:
		break;
	}

	return false;
}
