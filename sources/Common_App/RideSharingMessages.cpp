#include "RideSharingMessages.h"

#include <json/json.h>

#include <DecentApi/CommonApp/Net/MessageException.h>
#include "..\Common\RideSharingMessages.h"

constexpr char const FromPassenger::sk_ValueCat[];

Json::Value& FromPassenger::GetJsonMsg(Json::Value& outJson) const
{
	Json::Value& root = SmartMessages::GetJsonMsg(outJson);

	root = Json::nullValue;

	return root;
}

constexpr char const FromDriver::sk_ValueCat[];

Json::Value& FromDriver::GetJsonMsg(Json::Value& outJson) const
{
	Json::Value& root = SmartMessages::GetJsonMsg(outJson);

	root = Json::nullValue;

	return root;
}

constexpr char const FromTripPlaner::sk_ValueCat[];

Json::Value& FromTripPlaner::GetJsonMsg(Json::Value& outJson) const
{
	Json::Value& root = SmartMessages::GetJsonMsg(outJson);

	root = Json::nullValue;

	return root;
}

constexpr char const FromTripMatcher::sk_ValueCat[];

Json::Value& FromTripMatcher::GetJsonMsg(Json::Value& outJson) const
{
	Json::Value& root = SmartMessages::GetJsonMsg(outJson);

	root = Json::nullValue;

	return root;
}

constexpr char const FromPayment::sk_ValueCat[];

Json::Value& FromPayment::GetJsonMsg(Json::Value& outJson) const
{
	Json::Value& root = SmartMessages::GetJsonMsg(outJson);

	root = Json::nullValue;

	return root;
}
