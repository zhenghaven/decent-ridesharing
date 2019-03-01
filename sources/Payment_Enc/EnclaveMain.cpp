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

#include "../Common_Enc/OperatorPayment.h"
#include "../Common_Enc/Connector.h"

#include "Enclave_t.h"

using namespace RideShare;
using namespace Decent::Ra;

namespace
{
	static States& gs_state = States::Get();

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

static std::unique_ptr<ComMsg::RequestedPayment> GetPassengerPayment(const std::string& pasId)
{
	using namespace EncFunc::PassengerMgm;
	LOGI("Connecting to a Passenger Management...");

	Connector cnt(&ocall_ride_share_cnt_mgr_get_pas_mgm);
	if (!cnt.m_ptr)
	{
		return nullptr;
	}

	std::shared_ptr<Decent::Ra::TlsConfig> tlsCfg = std::make_shared<Decent::Ra::TlsConfig>(AppNames::sk_passengerMgm, gs_state, false);
	Decent::Net::TlsCommLayer tls(cnt.m_ptr, tlsCfg, true);

	std::string strBuf;
	if (!tls.SendMsg(cnt.m_ptr, EncFunc::GetMsgString(k_getPayInfo)) ||
		!tls.SendMsg(cnt.m_ptr, pasId) ||
		!tls.ReceiveMsg(cnt.m_ptr, strBuf))
	{
		return nullptr;
	}

	return ParseMsg<ComMsg::RequestedPayment>(strBuf);
}

static std::unique_ptr<ComMsg::RequestedPayment> GetDriverPayment(const std::string& driId)
{
	using namespace EncFunc::DriverMgm;
	LOGI("Connecting to a Driver Management...");

	Connector cnt(&ocall_ride_share_cnt_mgr_get_dri_mgm);
	if (!cnt.m_ptr)
	{
		return nullptr;
	}

	std::shared_ptr<Decent::Ra::TlsConfig> tlsCfg = std::make_shared<Decent::Ra::TlsConfig>(AppNames::sk_driverMgm, gs_state, false);
	Decent::Net::TlsCommLayer tls(cnt.m_ptr, tlsCfg, true);

	std::string strBuf;
	if (!tls.SendMsg(cnt.m_ptr, EncFunc::GetMsgString(k_getPayInfo)) ||
		!tls.SendMsg(cnt.m_ptr, driId) ||
		!tls.ReceiveMsg(cnt.m_ptr, strBuf))
	{
		return nullptr;
	}

	return ParseMsg<ComMsg::RequestedPayment>(strBuf);
}

static void ProcessPayment(void* const connection, Decent::Net::TlsCommLayer& tls)
{
	LOGI("Process payment...");

	std::string msgBuf;

	std::unique_ptr<ComMsg::FinalBill> bill;

	if (!tls.ReceiveMsg(connection, msgBuf) ||
		!(bill = ParseMsg<ComMsg::FinalBill>(msgBuf)))
	{
		return;
	}

	std::unique_ptr<ComMsg::RequestedPayment> pasPayment = GetPassengerPayment(bill->GetQuote().GetPasId());
	std::unique_ptr<ComMsg::RequestedPayment> driPayment = GetDriverPayment(bill->GetDriId());

	if (!pasPayment || !driPayment)
	{
		return;
	}

	PRINT_I("Pay from %s, to:", pasPayment->GetPayemnt().c_str());
	PRINT_I("\t Driver                       : %s", driPayment->GetPayemnt().c_str());
	PRINT_I("\t Billing Services     Operator: %s", bill->GetQuote().GetPrice().GetOpPayment().c_str());
	PRINT_I("\t Trip Planner         Operator: %s", bill->GetQuote().GetOpPayment().c_str());
	PRINT_I("\t Trip Matcher         Operator: %s", bill->GetOpPayment().c_str());
	PRINT_I("\t Passenger Management Operator: %s", pasPayment->GetOpPayment().c_str());
	PRINT_I("\t Driver Management    Operator: %s", driPayment->GetOpPayment().c_str());
	PRINT_I("\t Payment Services     Operator: %s", OperatorPayment::GetPaymentInfo().c_str());
}

extern "C" int ecall_ride_share_pay_from_trip_matcher(void* const connection)
{
	if (!OperatorPayment::IsPaymentInfoValid())
	{
		return false;
	}

	using namespace EncFunc::Payment;
	LOGI("Processing message from trip matcher...");

	std::shared_ptr<Decent::Ra::TlsConfig> tmTlsCfg = std::make_shared<Decent::Ra::TlsConfig>(AppNames::sk_tripMatcher, gs_state, true);
	Decent::Net::TlsCommLayer tmTls(connection, tmTlsCfg, true);

	std::string msgBuf;
	if (!tmTls.ReceiveMsg(connection, msgBuf) )
	{
		return false;
	}

	const NumType funcNum = EncFunc::GetNum<NumType>(msgBuf);

	LOGI("Request Function: %d.", funcNum);

	switch (funcNum)
	{
	case k_procPayment:
		ProcessPayment(connection, tmTls);
		break;
	default:
		break;
	}

	return false;
}
