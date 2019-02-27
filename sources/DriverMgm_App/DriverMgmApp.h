#pragma once

#include <DecentApi/DecentAppApp/DecentApp.h>

class DriverMgm : public Decent::RaSgx::DecentApp
{
public:
	using Decent::RaSgx::DecentApp::DecentApp;

	virtual ~DriverMgm() {}

	virtual bool ProcessMsgFromDriver(Decent::Net::Connection& connection);

	virtual bool ProcessMsgFromTripMatcher(Decent::Net::Connection& connection);

	virtual bool ProcessMsgFromPayment(Decent::Net::Connection& connection);

	virtual bool ProcessSmartMessage(const std::string& category, const Json::Value& jsonMsg, Decent::Net::Connection& connection) override;

private:

};
