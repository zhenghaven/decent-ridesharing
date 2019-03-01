#pragma once

#include "../Common_App/RideShareApp.h"

namespace RideShare
{
	class PaymentApp : public RideShareApp
	{
	public:
		using RideShareApp::RideShareApp;

		virtual ~PaymentApp() {}

		virtual bool ProcessMsgFromTripMatcher(Decent::Net::Connection& connection);

		virtual bool ProcessSmartMessage(const std::string& category, const Json::Value& jsonMsg, Decent::Net::Connection& connection) override;

	};
}
