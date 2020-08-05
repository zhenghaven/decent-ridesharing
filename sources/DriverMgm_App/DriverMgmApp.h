#pragma once

#include "../Common_App/RideShareApp.h"

namespace RideShare
{
	class DriverMgm : public RideShareApp
	{
	public:
		using RideShareApp::RideShareApp;

		virtual ~DriverMgm() {}

		virtual bool ProcessMsgFromDriver(Decent::Net::ConnectionBase& connection);

		virtual bool ProcessMsgFromTripMatcher(Decent::Net::ConnectionBase& connection);

		virtual bool ProcessMsgFromPayment(Decent::Net::ConnectionBase& connection);

		virtual bool ProcessSmartMessage(const std::string& category, Decent::Net::ConnectionBase& connection, Decent::Net::ConnectionBase*& freeHeldCnt) override;

	};
}
