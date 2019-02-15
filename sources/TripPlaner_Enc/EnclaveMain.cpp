//#include "Enclave_t.h"

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
}

static bool FindPath(const ComMsg::Point2D<double>& ori, const ComMsg::Point2D<double>& dst, std::vector<ComMsg::Point2D<double> >& path)
{
	path.clear();
	path.push_back(ori);
	path.push_back(dst);

	return true;
}

static void SendQueryLog(const std::string& userId, const ComMsg::GetQuote& getQuote)
{
	ComMsg::PasQueryLog log(userId, getQuote);

	void* pmCont = nullptr;
	//TODO: Get Connection to Billing Service

	LOGI("Connecting to a Passenger Management...");

	std::shared_ptr<Decent::Ra::TlsConfig> bsTlsCfg = std::make_shared<Decent::Ra::TlsConfig>(""/*TODO*/, gs_state, false);
	Decent::Net::TlsCommLayer pmTls(pmCont, bsTlsCfg, true);

	pmTls.SendMsg(pmCont, log.ToString());

	//TODO: Close connection.
}

static void ProcessGetQuote(void* const connection, Decent::Net::TlsCommLayer& tls)
{
	LOGI("Process Get Quote Request.");

	std::string msgBuf;
	rapidjson::Document json;

	try
	{
		if (!tls.ReceiveMsg(connection, msgBuf) ||
			!Decent::Tools::ParseStr2Json(json, msgBuf))
		{
			return;
		}

		ComMsg::GetQuote getQuote(json);
		json.Clear();

		SendQueryLog(""/*TODO*/, getQuote);

		LOGI("Finding Path...");
		std::vector<ComMsg::Point2D<double> > path;
		if (!FindPath(getQuote.GetOri(), getQuote.GetDest(), path))
		{
			return;
		}

		ComMsg::Path pathMsg(path);

		LOGI("Querying Billing Service for price...");

		void* bsCont = nullptr;

		//TODO: Get Connection to Billing Service

		LOGI("Connecting to a Billing Service...");
		std::shared_ptr<Decent::Ra::TlsConfig> bsTlsCfg = std::make_shared<Decent::Ra::TlsConfig>(""/*TODO*/, gs_state, false);
		Decent::Net::TlsCommLayer bsTls(bsCont, bsTlsCfg, true);
		LOGI("TLS Handshake Successful!");

		if (!bsTls.SendMsg(bsCont, pathMsg.ToString()) ||
			!bsTls.ReceiveMsg(bsCont, msgBuf) ||
			!Decent::Tools::ParseStr2Json(json, msgBuf))
		{
			return;
		}

		//TODO: Close connection.

		ComMsg::Price price(json);
		ComMsg::Quote quote(getQuote, path, price, "Trip_Planner_Part_1_Payment");
		LOGI("Constructing Signed Quote...");
		ComMsg::SignedQuote signedQuote = ComMsg::SignedQuote::SignQuote(quote, gs_state);

		tls.SendMsg(connection, signedQuote.ToString());

	}
	catch (const std::exception&)
	{
		return;
	}
}

static void ProcessConfirmQuote(void* const connection, Decent::Net::TlsCommLayer& tls)
{
	LOGI("Process Confirm Quote Request.");

	std::string msgBuf;
	rapidjson::Document json;

	try
	{
		if (!tls.ReceiveMsg(connection, msgBuf) ||
			!Decent::Tools::ParseStr2Json(json, msgBuf))
		{
			return;
		}

		ComMsg::ConfirmQuote confirmQuote(json);
		json.Clear();
		
		if (!Decent::Tools::ParseStr2Json(json, confirmQuote.GetSignQuote()))
		{
			return;
		}

		LOGI("Parsing Signed Quote...");
		ComMsg::SignedQuote signedQuote = ComMsg::SignedQuote::ParseSignedQuote(json, gs_state, ""/*TODO*/);
		json.Clear();

		if (!Decent::Tools::ParseStr2Json(json, signedQuote.GetQuote()))
		{
			return;
		}

		ComMsg::Quote quote(json);
		json.Clear();

		ComMsg::ConfirmedQuote confirmedQuote(confirmQuote.GetContact(), quote, "Trip_Planner_Part_2_Payment");
		
		LOGI("Sending confirmed quote to a Trip Matcher...");

		void* tmCont = nullptr;
		uint32_t tmIp = 0;
		uint16_t tmPort = 0;

		//TODO: Get Connection and address to Trip Matcher

		LOGI("Connecting to a Trip Matcher...");
		std::shared_ptr<Decent::Ra::TlsConfig> tmTlsCfg = std::make_shared<Decent::Ra::TlsConfig>(""/*TODO*/, gs_state, false);
		Decent::Net::TlsCommLayer tmTls(tmCont, tmTlsCfg, true);
		LOGI("TLS Handshake Successful!");

		if (!tmTls.SendMsg(tmCont, confirmedQuote.ToString()) ||
			!tmTls.ReceiveMsg(tmCont, msgBuf) ||
			!Decent::Tools::ParseStr2Json(json, msgBuf))
		{
			return;
		}

		//TODO: Close connection.
		
		ComMsg::TripId tripId(json);
		json.Clear();

		tls.SendMsg(connection, tripId.ToString());

	}
	catch (const std::exception&)
	{
		return;
	}
}

extern "C" int ecall_ride_share_tp_from_pas(void* const connection)
{
	LOGI("Processing message from passenger...");

	std::shared_ptr<RideShare::TlsConfig> tlsCfg = std::make_shared<RideShare::TlsConfig>(""/*TODO*/, gs_state, true);
	Decent::Net::TlsCommLayer tls(connection, tlsCfg, true);

	LOGI("TLS Handshake Successful!");

	std::string msgBuf;
	if (!tls.ReceiveMsg(connection, msgBuf) ||
		msgBuf.size() != sizeof(EncFunc::TripPlaner::NumType))
	{
		return false;
	}

	const EncFunc::TripPlaner::NumType funcNum = *reinterpret_cast<const EncFunc::TripPlaner::NumType*>(msgBuf.data());

	LOGI("Request Function: %d.", funcNum);

	switch (funcNum)
	{
	case EncFunc::TripPlaner::k_getQuote:
		ProcessGetQuote(connection, tls);
		break;
	case EncFunc::TripPlaner::k_confirmQuote:
		ProcessConfirmQuote(connection, tls);
		break;
	default:
		break;
	}

	return false;
}
