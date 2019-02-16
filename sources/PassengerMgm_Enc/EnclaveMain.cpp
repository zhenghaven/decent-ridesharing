//#include "Enclave_t.h"

#include <mutex>

#include <DecentApi/Common/Common.h>
#include <DecentApi/Common/Ra/TlsConfig.h>
#include <DecentApi/Common/Ra/States.h>
#include <DecentApi/Common/Ra/KeyContainer.h>
#include <DecentApi/Common/Ra/CertContainer.h>
#include <DecentApi/Common/Net/TlsCommLayer.h>
#include <DecentApi/Common/Tools/JsonTools.h>

#include <rapidjson/document.h>

#include "../Common/RideSharingFuncNums.h"
#include "../Common/RideSharingMessages.h"
#include "../Common/Crypto.h"
#include "../Common/TlsConfig.h"

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
			m_phone(rhs.m_phone),
			m_pay(rhs.m_pay)
		{}
	};

	std::map<std::string, PasProfileItem> gs_pasProfiles;
	const std::map<std::string, PasProfileItem>& gsk_pasProfiles = gs_pasProfiles;
	std::mutex gs_pasProfilesMutex;
}

static void ProcessPasRegisterReq(void* const connection, Decent::Net::TlsCommLayer& tls)
{
	LOGI("Processing Passenger Register Request...");

	std::string msgBuf;
	rapidjson::Document json;

	try
	{
		if (!tls.ReceiveMsg(connection, msgBuf) ||
			!Decent::Tools::ParseStr2Json(json, msgBuf))
		{
			return;
		}

		ComMsg::PasReg pasRegInfo(json);
		json.Clear();

		
		const std::string& csrPem = pasRegInfo.GetCsr();
		X509Req certReq(csrPem);

		std::shared_ptr<const Decent::MbedTlsObj::ECKeyPair> prvKey = gs_state.GetKeyContainer().GetSignKeyPair();
		std::shared_ptr<const AppX509> cert = std::dynamic_pointer_cast<const AppX509>(gs_state.GetCertContainer().GetCert());

		if (!certReq.VerifySignature() ||
			!prvKey || !*prvKey ||
			!cert || !*cert)
		{
			return;
		}
		
		ClientX509 clientCert(certReq.GetEcPublicKey(), *cert, *prvKey, "RideSharingClient");

		if (!clientCert)
		{
			LOGI("Client certificate generation failed!");
			return;
		}

		const std::string clientCertPem = clientCert.ToPemString();
		LOGI("Client certificate is generated:\n%s\n", clientCertPem.c_str());

		{
			std::unique_lock<std::mutex> pasProfilesLock(gs_pasProfilesMutex);
			auto it = gsk_pasProfiles.find(clientCertPem);
			if (it != gsk_pasProfiles.cend())
			{
				LOGI("Client profile already exist.");
				return;
			}

			gs_pasProfiles.insert(std::make_pair(clientCertPem,
				PasProfileItem(pasRegInfo.GetContact().GetName(), pasRegInfo.GetContact().GetPhone(), pasRegInfo.GetPayment())));

			LOGI("Client profile added. Profile store size: %llu.", gsk_pasProfiles.size());
		}

		tls.SendMsg(connection, clientCertPem);

	}
	catch (const std::exception&)
	{
		return;
	}
}

extern "C" int ecall_ride_share_pm_from_pas(void* const connection)
{
	LOGI("Processing message from passenger...");

	std::shared_ptr<Decent::Ra::TlsConfig> pasTlsCfg = std::make_shared<Decent::Ra::TlsConfig>(""/*TODO*/, gs_state, true);
	Decent::Net::TlsCommLayer pasTls(connection, pasTlsCfg, false);

	std::string msgBuf;
	if (!pasTls.ReceiveMsg(connection, msgBuf) ||
		msgBuf.size() != sizeof(EncFunc::PassengerMgm::NumType))
	{
		LOGI("TLS Handshake Failed!");
		return false;
	}

	LOGI("TLS Handshake Successful!");

	const EncFunc::PassengerMgm::NumType funcNum = *reinterpret_cast<const EncFunc::PassengerMgm::NumType*>(msgBuf.data());

	LOGI("Request Function: %d.", funcNum);

	switch (funcNum)
	{
	case EncFunc::PassengerMgm::k_userReg:
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
	rapidjson::Document json;

	try
	{
		if (!tls.ReceiveMsg(connection, msgBuf) ||
			!Decent::Tools::ParseStr2Json(json, msgBuf))
		{
			return;
		}

		ComMsg::PasQueryLog queryLog(json);
		json.Clear();

		const ComMsg::GetQuote& getQuote = queryLog.GetGetQuote();

		LOGI("Logged Passenger Query:");
		LOGI("Passenger ID:\n\t%s\n", queryLog.GetUserId().c_str());
		LOGI("Origin:      (%f, %f)\n", getQuote.GetOri().GetX(), getQuote.GetOri().GetY());
		LOGI("Destination: (%f, %f)\n", getQuote.GetDest().GetX(), getQuote.GetDest().GetY());

	}
	catch (const std::exception&)
	{
		return;
	}
}

extern "C" int ecall_ride_share_pm_from_trip_planner(void* const connection)
{
	LOGI("Processing message from Trip Planner...");

	std::shared_ptr<Decent::Ra::TlsConfig> tpTlsCfg = std::make_shared<Decent::Ra::TlsConfig>(""/*TODO*/, gs_state, true);
	Decent::Net::TlsCommLayer tpTls(connection, tpTlsCfg, true);

	std::string msgBuf;
	if (!tpTls.ReceiveMsg(connection, msgBuf) ||
		msgBuf.size() != sizeof(EncFunc::PassengerMgm::NumType))
	{
		LOGI("TLS Handshake Failed!");
		return false;
	}

	LOGI("TLS Handshake Successful!");

	const EncFunc::PassengerMgm::NumType funcNum = *reinterpret_cast<const EncFunc::PassengerMgm::NumType*>(msgBuf.data());

	LOGI("Request Function: %d.", funcNum);

	switch (funcNum)
	{
	case EncFunc::PassengerMgm::k_logQuery:
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
	rapidjson::Document json;

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
	LOGI("Processing message from payment services...");

	std::shared_ptr<Decent::Ra::TlsConfig> tpTlsCfg = std::make_shared<Decent::Ra::TlsConfig>(""/*TODO*/, gs_state, true);
	Decent::Net::TlsCommLayer tpTls(connection, tpTlsCfg, true);

	std::string msgBuf;
	if (!tpTls.ReceiveMsg(connection, msgBuf) ||
		msgBuf.size() != sizeof(EncFunc::PassengerMgm::NumType))
	{
		LOGI("TLS Handshake Failed!");
		return false;
	}

	LOGI("TLS Handshake Successful!");

	const EncFunc::PassengerMgm::NumType funcNum = *reinterpret_cast<const EncFunc::PassengerMgm::NumType*>(msgBuf.data());

	LOGI("Request Function: %d.", funcNum);

	switch (funcNum)
	{
	case EncFunc::PassengerMgm::k_getPayInfo:
		LogQuery(connection, tpTls);
		break;
	default:
		break;
	}

	return false;
}
