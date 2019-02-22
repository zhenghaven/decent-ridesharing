#include <cmath>

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
	
	std::string gs_selfPaymentInfo = "Billing Payment Info Test";

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
		LOGW("Failed to receive path! (Msg Buf: %s)", msgBuf.c_str());
		return;
	}

	const double price = CalPrice(pathMsg->GetPath());
	LOGI("Got price: %f.", price);

	ComMsg::Price priceMsg(price, gs_selfPaymentInfo);

	tls.SendMsg(connection, priceMsg.ToString());

}

extern "C" int ecall_ride_share_pm_from_trip_planner(void* const connection)
{
	using namespace EncFunc::Billing;

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
	case k_calPrice:
		ProcessCalPriceReq(connection, tpTls);
		break;
	default:
		break;
	}

	return false;
}
