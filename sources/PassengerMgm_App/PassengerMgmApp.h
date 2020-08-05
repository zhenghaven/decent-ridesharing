#pragma once

#include "../Common_App/RideShareApp.h"

namespace RideShare
{
	class PassengerMgm : public RideShareApp
	{
	public:
		using RideShareApp::RideShareApp;

		virtual ~PassengerMgm() {}

		virtual bool ProcessMsgFromPassenger(Decent::Net::ConnectionBase& connection);

		virtual bool ProcessMsgFromTripPlanner(Decent::Net::ConnectionBase& connection);

		virtual bool ProcessMsgFromPayment(Decent::Net::ConnectionBase& connection);

		virtual bool ProcessSmartMessage(const std::string& category, Decent::Net::ConnectionBase& connection, Decent::Net::ConnectionBase*& freeHeldCnt) override;

	};
}

