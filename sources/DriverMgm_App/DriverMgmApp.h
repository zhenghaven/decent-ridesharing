#pragma once

#include "../Common_App/RideShareApp.h"

namespace RideShare
{
	class DriverMgm : public RideShareApp
	{
	public:
		using RideShareApp::RideShareApp;

		virtual ~DriverMgm() {}

		virtual bool ProcessMsgFromDriver(Decent::Net::Connection& connection);

		virtual bool ProcessMsgFromTripMatcher(Decent::Net::Connection& connection);

		virtual bool ProcessMsgFromPayment(Decent::Net::Connection& connection);

		virtual bool ProcessSmartMessage(const std::string& category, const Json::Value& jsonMsg, Decent::Net::Connection& connection) override;

	};
}
