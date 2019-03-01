#pragma once

#include "../Common_App/RideShareApp.h"

namespace RideShare
{
	class TripPlanerApp : public RideShareApp
	{
	public:
		using RideShareApp::RideShareApp;

		virtual ~TripPlanerApp() {}

		virtual bool ProcessMsgFromPassenger(Decent::Net::Connection& connection);

		virtual bool ProcessSmartMessage(const std::string& category, const Json::Value& jsonMsg, Decent::Net::Connection& connection) override;

	};
}

