//#include "Enclave_t.h"

#include <mutex>

#include <DecentApi/Common/Common.h>
#include <DecentApi/Common/Ra/TlsConfig.h>
#include <DecentApi/Common/Ra/States.h>
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

		

		//tls.SendMsg(connection, tripId.ToString());

	}
	catch (const std::exception&)
	{
		return;
	}
}

extern "C" int ecall_ride_share_pm_from_pas(void* const connection)
{
	LOGI("Processing message from passenger...");

	std::shared_ptr<RideShare::TlsConfig> tlsCfg = std::make_shared<RideShare::TlsConfig>(""/*TODO*/, gs_state, true);
	Decent::Net::TlsCommLayer tls(connection, tlsCfg, true);

	LOGI("TLS Handshake Successful!");

	std::string msgBuf;
	if (!tls.ReceiveMsg(connection, msgBuf) ||
		msgBuf.size() != sizeof(EncFunc::PassengerMgm::NumType))
	{
		return false;
	}

	const EncFunc::PassengerMgm::NumType funcNum = *reinterpret_cast<const EncFunc::PassengerMgm::NumType*>(msgBuf.data());

	LOGI("Request Function: %d.", funcNum);

	switch (funcNum)
	{
	case EncFunc::PassengerMgm::k_userReg:
		ProcessPasRegisterReq(connection, tls);
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

	std::shared_ptr<Decent::Ra::TlsConfig> tpTlsCfg = std::make_shared<Decent::Ra::TlsConfig>(""/*TODO*/, gs_state, false);
	Decent::Net::TlsCommLayer tpTls(connection, tpTlsCfg, true);

	LOGI("TLS Handshake Successful!");

	std::string msgBuf;
	if (!tpTls.ReceiveMsg(connection, msgBuf) ||
		msgBuf.size() != sizeof(EncFunc::PassengerMgm::NumType))
	{
		return false;
	}

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

	std::shared_ptr<Decent::Ra::TlsConfig> tpTlsCfg = std::make_shared<Decent::Ra::TlsConfig>(""/*TODO*/, gs_state, false);
	Decent::Net::TlsCommLayer tpTls(connection, tpTlsCfg, true);

	LOGI("TLS Handshake Successful!");

	std::string msgBuf;
	if (!tpTls.ReceiveMsg(connection, msgBuf) ||
		msgBuf.size() != sizeof(EncFunc::PassengerMgm::NumType))
	{
		return false;
	}

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
