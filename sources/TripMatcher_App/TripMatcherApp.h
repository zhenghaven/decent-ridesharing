#pragma once

#include "../Common_App/RideShareApp.h"

namespace RideShare
{
	class TripMatcher : public RideShareApp
	{
	public:
		using RideShareApp::RideShareApp;

		virtual ~TripMatcher() {}

		virtual bool ProcessMsgFromPassenger(Decent::Net::ConnectionBase& connection);

		virtual bool ProcessMsgFromDriver(Decent::Net::ConnectionBase& connection);

		virtual bool ProcessSmartMessage(const std::string& category, Decent::Net::ConnectionBase& connection, Decent::Net::ConnectionBase*& freeHeldCnt) override;

	};
}
