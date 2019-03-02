#include <cmath>

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

static double CalDist(const ComMsg::Point2D<double>& a, const ComMsg::Point2D<double>& b)
{
	return sqrt(pow(b.GetX() - a.GetX(), 2) + pow(b.GetY() - a.GetY(), 2));
}

static double CalPrice(const std::vector<ComMsg::Point2D<double> >& path)
{
	constexpr double unitPrice = 1.5;
	double price = 0.0;
	if (path.size() == 0 || path.size() == 1)
	{
		return price;
	}

	const ComMsg::Point2D<double>* prevPoint = &path[0];
	for (size_t i = 1; i < path.size(); ++i)
	{
		price += CalDist(*prevPoint, path[i]) * unitPrice;
		prevPoint = &path[i];
	}

	return price;
}

static void ProcessCalPriceReq(void* const connection, Decent::Net::TlsCommLayer& tls)
{
	LOGI("Processing calculate price request...");

	std::string msgBuf;

	std::unique_ptr<ComMsg::Path> pathMsg;
	if (!tls.ReceiveMsg(connection, msgBuf) ||
		!(pathMsg = ParseMsg<ComMsg::Path>(msgBuf)))
	{
		return;
	}

	const double price = CalPrice(pathMsg->GetPath());
	LOGI("Got price: %f.", price);

	ComMsg::Price priceMsg(price, OperatorPayment::GetPaymentInfo());

	tls.SendMsg(connection, priceMsg.ToString());

}

extern "C" int ecall_ride_share_bill_from_trip_planner(void* const connection)
{
	if (!OperatorPayment::IsPaymentInfoValid())
	{
		return false;
	}

	using namespace EncFunc::Billing;

	LOGI("Processing message from Trip Planner...");

	std::shared_ptr<Decent::Ra::TlsConfig> tpTlsCfg = std::make_shared<Decent::Ra::TlsConfig>(AppNames::sk_tripPlanner, gs_state, true);
	Decent::Net::TlsCommLayer tpTls(connection, tpTlsCfg, true);

	std::string msgBuf;
	if (!tpTls.ReceiveMsg(connection, msgBuf) )
	{
		return false;
	}

	const NumType funcNum = EncFunc::GetNum<NumType>(msgBuf);

	LOGI("Request Function: %d.", funcNum);

	switch (funcNum)
	{
	case k_calPrice:
		ProcessCalPriceReq(connection, tpTls);
		break;
	default:
		break;
	}

	return false;
}
