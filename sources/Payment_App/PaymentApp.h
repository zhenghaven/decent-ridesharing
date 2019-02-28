#pragma once

#include <DecentApi/DecentAppApp/DecentApp.h>

class PaymentApp : public Decent::RaSgx::DecentApp
{
public:
	using Decent::RaSgx::DecentApp::DecentApp;

	virtual ~PaymentApp() {}

	virtual bool ProcessMsgFromTripMatcher(Decent::Net::Connection& connection);

	virtual bool ProcessSmartMessage(const std::string& category, const Json::Value& jsonMsg, Decent::Net::Connection& connection) override;

private:

};
