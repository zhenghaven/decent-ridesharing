#include <DecentApi/Common/Common.h>
#include <DecentApi/Common/make_unique.h>
#include <DecentApi/Common/Ra/TlsConfigWithName.h>
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

static std::unique_ptr<ComMsg::RequestedPayment> GetPassengerPayment(const std::string& pasId)
{
	using namespace EncFunc::PassengerMgm;
	LOGI("Connecting to a Passenger Management...");

	OcallConnector cnt(&ocall_ride_share_cnt_mgr_get_pas_mgm);

	std::shared_ptr<TlsConfigWithName> tlsCfg = std::make_shared<TlsConfigWithName>(gs_state, TlsConfig::Mode::ClientHasCert, AppNames::sk_passengerMgm);
	TlsCommLayer tls(cnt.Get(), tlsCfg, true);

	std::string strBuf;
	tls.SendStruct(cnt.Get(), k_getPayInfo);
	tls.SendMsg(cnt.Get(), pasId);
	tls.ReceiveMsg(cnt.Get(), strBuf);

	return ParseMsg<ComMsg::RequestedPayment>(strBuf);
}

static std::unique_ptr<ComMsg::RequestedPayment> GetDriverPayment(const std::string& driId)
{
	using namespace EncFunc::DriverMgm;
	LOGI("Connecting to a Driver Management...");

	OcallConnector cnt(&ocall_ride_share_cnt_mgr_get_dri_mgm);

	std::shared_ptr<TlsConfigWithName> tlsCfg = std::make_shared<TlsConfigWithName>(gs_state, TlsConfig::Mode::ClientHasCert, AppNames::sk_driverMgm);
	TlsCommLayer tls(cnt.Get(), tlsCfg, true);

	std::string strBuf;
	tls.SendStruct(cnt.Get(), k_getPayInfo);
	tls.SendMsg(cnt.Get(), driId);
	tls.ReceiveMsg(cnt.Get(), strBuf);

	return ParseMsg<ComMsg::RequestedPayment>(strBuf);
}

static void ProcessPayment(void* const connection, Decent::Net::TlsCommLayer& tls)
{
	LOGI("Process payment...");

	std::string msgBuf;
	
	tls.ReceiveMsg(connection, msgBuf);
	std::unique_ptr<ComMsg::FinalBill> bill = ParseMsg<ComMsg::FinalBill>(msgBuf);

	std::unique_ptr<ComMsg::RequestedPayment> pasPayment = GetPassengerPayment(bill->GetQuote().GetPasId());
	std::unique_ptr<ComMsg::RequestedPayment> driPayment = GetDriverPayment(bill->GetDriId());

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
	LOGI("Processing message from Trip Matcher...");

	try
	{
		std::shared_ptr<TlsConfigWithName> tlsCfg = std::make_shared<TlsConfigWithName>(gs_state, TlsConfig::Mode::ServerVerifyPeer, AppNames::sk_tripMatcher);
		TlsCommLayer tls(connection, tlsCfg, true);

		NumType funcNum;
		tls.ReceiveStruct(connection, funcNum);

		switch (funcNum)
		{
		case k_procPayment:
			ProcessPayment(connection, tls);
			break;
		default:
			break;
		}
	}
	catch (const std::exception& e)
	{
		PRINT_W("Failed to processing message from Trip Matcher. Caught exception: %s", e.what());
	}

	return false;
}
