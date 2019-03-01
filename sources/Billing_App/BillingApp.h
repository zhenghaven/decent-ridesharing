#pragma once

#include "../Common_App/RideShareApp.h"

namespace RideShare
{
	class Billing : public RideShareApp
	{
	public:
		using RideShareApp::RideShareApp;

		virtual ~Billing() {}

		virtual bool ProcessMsgFromTripPlanner(Decent::Net::Connection& connection);

		virtual bool ProcessSmartMessage(const std::string& category, const Json::Value& jsonMsg, Decent::Net::Connection& connection) override;

	};
}
