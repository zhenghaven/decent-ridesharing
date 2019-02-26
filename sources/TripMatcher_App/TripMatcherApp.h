#pragma once

#include <DecentApi/DecentAppApp/DecentApp.h>

class TripMatcher : public Decent::RaSgx::DecentApp
{
public:
	using Decent::RaSgx::DecentApp::DecentApp;

	virtual ~TripMatcher() {}

	virtual bool ProcessMsgFromPassenger(Decent::Net::Connection& connection);

	virtual bool ProcessMsgFromDriver(Decent::Net::Connection& connection);

	virtual bool ProcessSmartMessage(const std::string& category, const Json::Value& jsonMsg, Decent::Net::Connection& connection) override;

private:

};
