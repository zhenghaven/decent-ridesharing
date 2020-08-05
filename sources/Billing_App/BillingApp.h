#pragma once

#include "../Common_App/RideShareApp.h"

namespace RideShare
{
	class Billing : public RideShareApp
	{
	public:
		using RideShareApp::RideShareApp;

		virtual ~Billing() {}

		virtual bool ProcessMsgFromTripPlanner(Decent::Net::ConnectionBase& connection);

		virtual bool ProcessSmartMessage(const std::string& category, Decent::Net::ConnectionBase& connection, Decent::Net::ConnectionBase*& freeHeldCnt) override;

	};
}
