#include "RideSharingMessages.h"

#include <json/json.h>

#include <DecentApi/CommonApp/Net/MessageException.h>
#include "..\Common\RideSharingMessages.h"

constexpr char const GetQuoteMessage::sk_ValueCat[];

Json::Value& GetQuoteMessage::GetJsonMsg(Json::Value& outJson) const
{
	Json::Value& root = SmartMessages::GetJsonMsg(outJson);

	root = Json::nullValue;

	return root;
}
