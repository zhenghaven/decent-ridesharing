#pragma once

#include <DecentApi/DecentAppApp/DecentApp.h>

class Billing : public Decent::RaSgx::DecentApp
{
public:
	using Decent::RaSgx::DecentApp::DecentApp;

	virtual ~Billing() {}

	virtual bool ProcessMsgFromTripPlanner(Decent::Net::Connection& connection);

	virtual bool ProcessSmartMessage(const std::string& category, const Json::Value& jsonMsg, Decent::Net::Connection& connection) override;

private:

};
