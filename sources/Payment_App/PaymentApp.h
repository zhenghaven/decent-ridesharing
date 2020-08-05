#pragma once

#include "../Common_App/RideShareApp.h"

namespace RideShare
{
	class PaymentApp : public RideShareApp
	{
	public:
		using RideShareApp::RideShareApp;

		virtual ~PaymentApp() {}

		virtual bool ProcessMsgFromTripMatcher(Decent::Net::ConnectionBase& connection);

		virtual bool ProcessSmartMessage(const std::string& category, Decent::Net::ConnectionBase& connection, Decent::Net::ConnectionBase*& freeHeldCnt) override;

	};
}
