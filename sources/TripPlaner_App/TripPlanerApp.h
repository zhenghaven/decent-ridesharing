#pragma once

#include <DecentApi/DecentAppApp/DecentApp.h>

class TripPlanerApp : public Decent::RaSgx::DecentApp
{
public:
	using Decent::RaSgx::DecentApp::DecentApp;

	virtual ~TripPlanerApp() {}

	virtual bool ProcessMsgFromPassenger(Decent::Net::Connection& connection);

	virtual bool ProcessSmartMessage(const std::string& category, const Json::Value& jsonMsg, Decent::Net::Connection& connection) override;

private:

};
