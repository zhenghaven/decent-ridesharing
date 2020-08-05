#include <cmath>

#include <mutex>
#include <memory>

#include <DecentApi/Common/Common.h>
#include <DecentApi/Common/make_unique.h>
#include <DecentApi/Common/Ra/TlsConfigWithName.h>
#include <DecentApi/Common/Ra/KeyContainer.h>
#include <DecentApi/Common/Ra/CertContainer.h>
#include <DecentApi/Common/Net/TlsCommLayer.h>
#include <DecentApi/Common/Tools/JsonTools.h>
#include <DecentApi/CommonEnclave/Net/EnclaveCntTranslator.h>
#include <DecentApi/DecentAppEnclave/AppStatesSingleton.h>

#include <rapidjson/document.h>

#include "../Common/AppNames.h"
#include "../Common/RideSharingFuncNums.h"
#include "../Common/RideSharingMessages.h"

#include "../Common_Enc/OperatorPayment.h"

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

	EnclaveCntTranslator cnt(connection);

	std::string msgBuf = tls.RecvContainer<std::string>(cnt);
	std::unique_ptr<ComMsg::Path> pathMsg = ParseMsg<ComMsg::Path>(msgBuf);

	const double price = CalPrice(pathMsg->GetPath());
	LOGI("Got price: %f.", price);

	ComMsg::Price priceMsg(price, OperatorPayment::GetPaymentInfo());

	tls.SendContainer(cnt, priceMsg.ToString());

}

extern "C" int ecall_ride_share_bill_from_trip_planner(void* const connection)
{
	if (!OperatorPayment::IsPaymentInfoValid())
	{
		return false;
	}

	using namespace EncFunc::Billing;

	LOGI("Processing message from Trip Planner...");

	EnclaveCntTranslator cnt(connection);

	try
	{
		std::shared_ptr<TlsConfigWithName> tlsCfg = std::make_shared<TlsConfigWithName>(gs_state, TlsConfigWithName::Mode::ServerVerifyPeer, AppNames::sk_tripPlanner, nullptr);
		TlsCommLayer tls(cnt, tlsCfg, true, nullptr);

		NumType funcNum;
		tls.RecvStruct(cnt, funcNum);

		switch (funcNum)
		{
		case k_calPrice:
			ProcessCalPriceReq(connection, tls);
			break;
		default:
			break;
		}
	}
	catch (const std::exception& e)
	{
		PRINT_W("Failed to processing message from Trip Planner. Caught exception: %s", e.what());
	}

	return false;
}
