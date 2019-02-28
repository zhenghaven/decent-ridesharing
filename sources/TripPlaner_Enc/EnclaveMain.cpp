//#include "Enclave_t.h"

#include <DecentApi/Common/Common.h>
#include <DecentApi/Common/make_unique.h>
#include <DecentApi/Common/Ra/TlsConfig.h>
#include <DecentApi/Common/Ra/States.h>
#include <DecentApi/Common/Net/TlsCommLayer.h>
#include <DecentApi/Common/Tools/JsonTools.h>

#include <rapidjson/document.h>

#include "../Common/RideSharingFuncNums.h"
#include "../Common/RideSharingMessages.h"
#include "../Common/Crypto.h"
#include "../Common/TlsConfig.h"
#include "../Common/AppNames.h"

#include "Enclave_t.h"

using namespace RideShare;
using namespace Decent::Ra;

namespace
{
	static States& gs_state = States::Get();

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

	typedef sgx_status_t(*CntBuilderType)(void**);

	struct CntWraper
	{
		CntWraper(CntBuilderType cntBuilder) :
			m_ptr(nullptr)
		{
			if ((*cntBuilder)(&m_ptr) != SGX_SUCCESS)
			{
				m_ptr = nullptr;
			}
		}

		~CntWraper()
		{
			ocall_ride_share_cnt_mgr_close_cnt(m_ptr);
		}
		void* m_ptr;
	};
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
	ComMsg::PasQueryLog log(userId, getQuote);

	LOGI("Connecting to a Passenger Management...");

	CntWraper pasMgmCnt(&ocall_ride_share_cnt_mgr_get_pas_mgm);
	if (!pasMgmCnt.m_ptr)
	{
		return false;
	}

	std::shared_ptr<Decent::Ra::TlsConfig> pasMgmTlsCfg = std::make_shared<Decent::Ra::TlsConfig>(AppNames::sk_passengerMgm, gs_state, false);
	Decent::Net::TlsCommLayer pasMgmTls(pasMgmCnt.m_ptr, pasMgmTlsCfg, true);

	return pasMgmTls.SendMsg(pasMgmCnt.m_ptr, EncFunc::GetMsgString(EncFunc::PassengerMgm::k_logQuery)) &&
		pasMgmTls.SendMsg(pasMgmCnt.m_ptr, log.ToString());
}

static std::unique_ptr<ComMsg::Price> GetPriceFromBilling(const ComMsg::Path& pathMsg)
{
	LOGI("Connecting to a Billing Service...");
	CntWraper billingCnt(&ocall_ride_share_cnt_mgr_get_billing);
	if (!billingCnt.m_ptr)
	{
		LOGW("Failed to Connect to a Billing Service!");
		return nullptr;
	}

	std::shared_ptr<Decent::Ra::TlsConfig> bsTlsCfg = std::make_shared<Decent::Ra::TlsConfig>(AppNames::sk_billing, gs_state, false);
	Decent::Net::TlsCommLayer bsTls(billingCnt.m_ptr, bsTlsCfg, true);

	std::string msgBuf;
	std::unique_ptr<ComMsg::Price> price;
	if (!bsTls.SendMsg(billingCnt.m_ptr, EncFunc::GetMsgString(EncFunc::Billing::k_calPrice)) ||
		!bsTls.SendMsg(billingCnt.m_ptr, pathMsg.ToString()) ||
		!bsTls.ReceiveMsg(billingCnt.m_ptr, msgBuf) ||
		!(price = ParseMsg<ComMsg::Price>(msgBuf)))
	{
		LOGW("Failed to receive price! (Msg Buf: %s)", msgBuf.c_str());
		return nullptr;
	}

	return std::move(price);
}

static void ProcessGetQuote(void* const connection, Decent::Net::TlsCommLayer& tls)
{
	LOGI("Process Get Quote Request.");

	std::string msgBuf;

	std::unique_ptr<ComMsg::GetQuote> getQuote;

	if (!tls.ReceiveMsg(connection, msgBuf) ||
		!(getQuote = ParseMsg<ComMsg::GetQuote>(msgBuf)))
	{
		return;
	}

	ClientX509 peerCert = tls.GetPeerCertPem();
	if (!peerCert)
	{
		return;
	}

	if (!SendQueryLog(peerCert.GetEcPublicKey().ToPubPemString(), *getQuote))
	{
		return;
	}

	LOGI("Finding Path...");
	std::vector<ComMsg::Point2D<double> > path;
	if (!FindPath(getQuote->GetOri(), getQuote->GetDest(), path))
	{
		return;
	}

	ComMsg::Path pathMsg(std::move(path));

	LOGI("Querying Billing Service for price...");
	std::unique_ptr<ComMsg::Price> price = GetPriceFromBilling(pathMsg);
	if (!price)
	{
		return;
	}

	ComMsg::Quote quote(*getQuote, pathMsg, *price, "Trip_Planner_Part_1_Payment");
	LOGI("Constructing Signed Quote...");
	ComMsg::SignedQuote signedQuote = ComMsg::SignedQuote::SignQuote(quote, gs_state);

	tls.SendMsg(connection, signedQuote.ToString());

}

extern "C" int ecall_ride_share_tp_from_pas(void* const connection)
{
	using namespace EncFunc::TripPlaner;
	LOGI("Processing message from passenger...");

	std::shared_ptr<RideShare::TlsConfig> tlsCfg = std::make_shared<RideShare::TlsConfig>(AppNames::sk_passengerMgm, gs_state, true);
	Decent::Net::TlsCommLayer tls(connection, tlsCfg, true);

	std::string msgBuf;
	if (!tls.ReceiveMsg(connection, msgBuf) ||
		msgBuf.size() != sizeof(EncFunc::TripPlaner::NumType))
	{
		LOGW("Recv size: %llu", msgBuf.size());
		return false;
	}

	LOGI("TLS Handshake Successful!");

	const NumType funcNum = EncFunc::GetNum<NumType>(msgBuf);

	LOGI("Request Function: %d.", funcNum);

	switch (funcNum)
	{
	case k_getQuote:
		ProcessGetQuote(connection, tls);
		break;
	default:
		break;
	}

	return false;
}
