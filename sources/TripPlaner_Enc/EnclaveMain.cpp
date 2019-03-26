//#include "Enclave_t.h"

#include <DecentApi/Common/Common.h>
#include <DecentApi/Common/make_unique.h>
#include <DecentApi/Common/Ra/TlsConfigWithName.h>
#include <DecentApi/Common/Ra/TlsConfigClient.h>
#include <DecentApi/Common/Net/TlsCommLayer.h>
#include <DecentApi/Common/Tools/JsonTools.h>
#include <DecentApi/CommonEnclave/SGX/OcallConnector.h>
#include <DecentApi/DecentAppEnclave/AppStatesSingleton.h>

#include <rapidjson/document.h>

#include "../Common/RideSharingFuncNums.h"
#include "../Common/RideSharingMessages.h"
#include "../Common/AppNames.h"

#include "../Common_Enc/OperatorPayment.h"

#include "Enclave_t.h"

using namespace RideShare;
using namespace Decent::Ra;
using namespace Decent::Net;

namespace
{
	static AppStates& gs_state = GetAppStateSingleton();

	template<typename MsgType>
	static std::unique_ptr<MsgType> ParseMsg(const std::string& msgStr)
	{
		rapidjson::Document json;
		Decent::Tools::ParseStr2Json(json, msgStr);
		return Decent::Tools::make_unique<MsgType>(json);
	}
}

static bool FindPath(const ComMsg::Point2D<double>& ori, const ComMsg::Point2D<double>& dst, std::vector<ComMsg::Point2D<double> >& path)
{
	path.clear();
	path.push_back(ori);
	path.push_back(dst);

	return true;
}

static bool SendQueryLog(const std::string& userId, const ComMsg::GetQuote& getQuote)
{
	using namespace EncFunc::PassengerMgm;
	LOGI("Connecting to a Passenger Management...");

	OcallConnector cnt(&ocall_ride_share_cnt_mgr_get_pas_mgm);

	ComMsg::PasQueryLog log(userId, getQuote);

	std::shared_ptr<TlsConfigWithName> pasMgmTlsCfg = std::make_shared<TlsConfigWithName>(gs_state, TlsConfig::Mode::ClientHasCert, AppNames::sk_passengerMgm);
	Decent::Net::TlsCommLayer pasMgmTls(cnt.Get(), pasMgmTlsCfg, true);
	
	pasMgmTls.SendStruct(cnt.Get(), k_logQuery);
	pasMgmTls.SendMsg(cnt.Get(), log.ToString());

	return true;
}

static std::unique_ptr<ComMsg::Price> GetPriceFromBilling(const ComMsg::Path& pathMsg)
{
	using namespace EncFunc::Billing;
	LOGI("Querying Billing Service for price...");
	OcallConnector cnt(&ocall_ride_share_cnt_mgr_get_billing);

	std::shared_ptr<TlsConfigWithName> bsTlsCfg = std::make_shared<TlsConfigWithName>(gs_state, TlsConfig::Mode::ClientHasCert, AppNames::sk_billing);
	Decent::Net::TlsCommLayer bsTls(cnt.Get(), bsTlsCfg, true);

	std::string msgBuf;

	bsTls.SendStruct(cnt.Get(), k_calPrice);
	bsTls.SendMsg(cnt.Get(), pathMsg.ToString());
	bsTls.ReceiveMsg(cnt.Get(), msgBuf);
	std::unique_ptr<ComMsg::Price> price = ParseMsg<ComMsg::Price>(msgBuf);

	return std::move(price);
}

static void ProcessGetQuote(void* const connection, Decent::Net::TlsCommLayer& tls)
{
	LOGI("Process Get Quote Request.");

	std::string msgBuf;
	
	tls.ReceiveMsg(connection, msgBuf);
	std::unique_ptr<ComMsg::GetQuote> getQuote = ParseMsg<ComMsg::GetQuote>(msgBuf);

	const std::string pasId = tls.GetPublicKeyPem();
	if (pasId.size() == 0 ||
		!SendQueryLog(pasId, *getQuote))
	{
		return;
	}

	std::vector<ComMsg::Point2D<double> > path;
	if (!FindPath(getQuote->GetOri(), getQuote->GetDest(), path))
	{
		return;
	}

	ComMsg::Path pathMsg(std::move(path));

	std::unique_ptr<ComMsg::Price> price = GetPriceFromBilling(pathMsg);
	if (!price)
	{
		return;
	}

	ComMsg::Quote quote(*getQuote, pathMsg, *price, OperatorPayment::GetPaymentInfo(), pasId);
	ComMsg::SignedQuote signedQuote = ComMsg::SignedQuote::SignQuote(quote, gs_state);

	tls.SendMsg(connection, signedQuote.ToString());

}

extern "C" int ecall_ride_share_tp_from_pas(void* const connection)
{
	if (!OperatorPayment::IsPaymentInfoValid())
	{
		return false;
	}

	using namespace EncFunc::TripPlaner;
	LOGI("Processing message from passenger...");

	try
	{
		std::shared_ptr<TlsConfigClient> tlsCfg = std::make_shared<TlsConfigClient>(gs_state, TlsConfig::Mode::ServerVerifyPeer, AppNames::sk_passengerMgm);
		TlsCommLayer tls(connection, tlsCfg, true);

		NumType funcNum;
		tls.ReceiveStruct(connection, funcNum);

		switch (funcNum)
		{
		case k_getQuote:
			ProcessGetQuote(connection, tls);
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
