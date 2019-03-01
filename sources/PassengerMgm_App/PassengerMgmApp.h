#pragma once

#include "../Common_App/RideShareApp.h"

namespace RideShare
{
	class PassengerMgm : public RideShareApp
	{
	public:
		using RideShareApp::RideShareApp;

		virtual ~PassengerMgm() {}

		virtual bool ProcessMsgFromPassenger(Decent::Net::Connection& connection);

		virtual bool ProcessMsgFromTripPlanner(Decent::Net::Connection& connection);

		virtual bool ProcessMsgFromPayment(Decent::Net::Connection& connection);

		virtual bool ProcessSmartMessage(const std::string& category, const Json::Value& jsonMsg, Decent::Net::Connection& connection) override;

	};
}

