#pragma once

#include <DecentApi/CommonApp/Net/SmartMessages.h>

class GetQuoteMessage : public Decent::Net::SmartMessages
{
public:
	static constexpr char const sk_ValueCat[] =  "RideSharingGetQuote";

public:
	GetQuoteMessage() :
		SmartMessages()
	{}

	GetQuoteMessage(const Json::Value& msg) :
		SmartMessages(msg, sk_ValueCat)
	{}

	~GetQuoteMessage()
	{}

	virtual std::string GetMessageCategoryStr() const override { return sk_ValueCat; }

protected:
	virtual Json::Value& GetJsonMsg(Json::Value& outJson) const override;

};
