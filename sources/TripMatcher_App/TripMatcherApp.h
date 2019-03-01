#pragma once

#include "../Common_App/RideShareApp.h"

namespace RideShare
{
	class TripMatcher : public RideShareApp
	{
	public:
		using RideShareApp::RideShareApp;

		virtual ~TripMatcher() {}

		virtual bool ProcessMsgFromPassenger(Decent::Net::Connection& connection);

		virtual bool ProcessMsgFromDriver(Decent::Net::Connection& connection);

		virtual bool ProcessSmartMessage(const std::string& category, const Json::Value& jsonMsg, Decent::Net::Connection& connection) override;

	};
}
