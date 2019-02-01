#include "TripPlanerApp.h"

#include <cstdio>

#include "../Common_App/RideSharingMessages.h"

bool TripPlanerApp::ProcessQuoteRequest(Decent::Net::Connection & connection)
{
	return false;
}

bool TripPlanerApp::ProcessConfirmedQuote(Decent::Net::Connection & connection)
{
	return false;
}

bool TripPlanerApp::ProcessSmartMessage(const std::string & category, const Json::Value & jsonMsg, Decent::Net::Connection & connection)
{
	if (category == GetQuoteMessage::sk_ValueCat)
	{
		return ProcessQuoteRequest(connection);
	}
	else
	{
		return Decent::RaSgx::DecentApp::ProcessSmartMessage(category, jsonMsg, connection);
	}
}
