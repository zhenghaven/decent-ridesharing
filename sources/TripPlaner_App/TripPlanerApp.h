#pragma once

#include "../Common_App/RideShareApp.h"

namespace RideShare
{
	class TripPlanerApp : public RideShareApp
	{
	public:
		using RideShareApp::RideShareApp;

		virtual ~TripPlanerApp() {}

		virtual bool ProcessMsgFromPassenger(Decent::Net::ConnectionBase& connection);

		virtual bool ProcessSmartMessage(const std::string& category, Decent::Net::ConnectionBase& connection, Decent::Net::ConnectionBase*& freeHeldCnt) override;

	};
}

