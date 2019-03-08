//#include "Enclave_t.h"

#include <DecentApi/Common/Common.h>
#include <DecentApi/Common/make_unique.h>
#include <DecentApi/Common/Ra/TlsConfig.h>
#include <DecentApi/Common/Net/TlsCommLayer.h>
#include <DecentApi/Common/Tools/JsonTools.h>
#include <DecentApi/DecentAppEnclave/AppStatesSingleton.h>

#include <rapidjson/document.h>

#include "../Common/RideSharingFuncNums.h"
#include "../Common/RideSharingMessages.h"
#include "../Common/Crypto.h"
#include "../Common/TlsConfig.h"
#include "../Common/AppNames.h"

#include "../Common_Enc/OperatorPayment.h"
#include "../Common_Enc/Connector.h"

#include "Enclave_t.h"

using namespace RideShare;
using namespace Decent::Ra;

namespace
{
	static AppStates& gs_state = GetAppStateSingleton();

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

	Connector cnt(&ocall_ride_share_cnt_mgr_get_pas_mgm);
	if (!cnt.m_ptr)
	{
		return false;
	}

	ComMsg::PasQueryLog log(userId, getQuote);

	std::shared_ptr<Decent::Ra::TlsConfig> pasMgmTlsCfg = std::make_shared<Decent::Ra::TlsConfig>(AppNames::sk_passengerMgm, gs_state, false);
	Decent::Net::TlsCommLayer pasMgmTls(cnt.m_ptr, pasMgmTlsCfg, true);
	
	pasMgmTls.SendStruct(cnt.m_ptr, k_logQuery);
	pasMgmTls.SendMsg(cnt.m_ptr, log.ToString());

	return true;
}

static std::unique_ptr<ComMsg::Price> GetPriceFromBilling(const ComMsg::Path& pathMsg)
{
	using namespace EncFunc::Billing;
	LOGI("Querying Billing Service for price...");
	Connector cnt(&ocall_ride_share_cnt_mgr_get_billing);
	if (!cnt.m_ptr)
	{
		return nullptr;
	}

	std::shared_ptr<Decent::Ra::TlsConfig> bsTlsCfg = std::make_shared<Decent::Ra::TlsConfig>(AppNames::sk_billing, gs_state, false);
	Decent::Net::TlsCommLayer bsTls(cnt.m_ptr, bsTlsCfg, true);

	std::string msgBuf;
	std::unique_ptr<ComMsg::Price> price;

	bsTls.SendStruct(cnt.m_ptr, k_calPrice);
	bsTls.SendMsg(cnt.m_ptr, pathMsg.ToString());
	bsTls.ReceiveMsg(cnt.m_ptr, msgBuf);
	if (!(price = ParseMsg<ComMsg::Price>(msgBuf)))
	{
		return nullptr;
	}

	return std::move(price);
}

static void ProcessGetQuote(void* const connection, Decent::Net::TlsCommLayer& tls)
{
	LOGI("Process Get Quote Request.");

	std::string msgBuf;

	std::unique_ptr<ComMsg::GetQuote> getQuote;
	
	tls.ReceiveMsg(connection, msgBuf);
	if (!(getQuote = ParseMsg<ComMsg::GetQuote>(msgBuf)))
	{
		return;
	}

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
		std::shared_ptr<RideShare::TlsConfig> tlsCfg = std::make_shared<RideShare::TlsConfig>(AppNames::sk_passengerMgm, gs_state, true);
		Decent::Net::TlsCommLayer tls(connection, tlsCfg, true);

		NumType funcNum;
		tls.ReceiveStruct(connection, funcNum);

		LOGI("Request Function: %d.", funcNum);

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
