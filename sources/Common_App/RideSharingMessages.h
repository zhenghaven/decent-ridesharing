#pragma once

#include <DecentApi/CommonApp/Net/SmartMessages.h>

class FromPassenger : public Decent::Net::SmartMessages
{
public:
	static constexpr char const sk_ValueCat[] =  "FromPassenger";

public:
	FromPassenger() :
		SmartMessages()
	{}

	FromPassenger(const Json::Value& msg) :
		SmartMessages(msg, sk_ValueCat)
	{}

	~FromPassenger()
	{}

	virtual std::string GetMessageCategoryStr() const override { return sk_ValueCat; }

protected:
	virtual Json::Value& GetJsonMsg(Json::Value& outJson) const override;

};

class FromDriver : public Decent::Net::SmartMessages
{
public:
	static constexpr char const sk_ValueCat[] = "FromDriver";

public:
	FromDriver() :
		SmartMessages()
	{}

	FromDriver(const Json::Value& msg) :
		SmartMessages(msg, sk_ValueCat)
	{}

	~FromDriver()
	{}

	virtual std::string GetMessageCategoryStr() const override { return sk_ValueCat; }

protected:
	virtual Json::Value& GetJsonMsg(Json::Value& outJson) const override;

};

class FromTripPlaner : public Decent::Net::SmartMessages
{
public:
	static constexpr char const sk_ValueCat[] = "FromTripPlaner";

public:
	FromTripPlaner() :
		SmartMessages()
	{}

	FromTripPlaner(const Json::Value& msg) :
		SmartMessages(msg, sk_ValueCat)
	{}

	~FromTripPlaner()
	{}

	virtual std::string GetMessageCategoryStr() const override { return sk_ValueCat; }

protected:
	virtual Json::Value& GetJsonMsg(Json::Value& outJson) const override;

};

class FromTripMatcher : public Decent::Net::SmartMessages
{
public:
	static constexpr char const sk_ValueCat[] = "FromTripMatcher";

public:
	FromTripMatcher() :
		SmartMessages()
	{}

	FromTripMatcher(const Json::Value& msg) :
		SmartMessages(msg, sk_ValueCat)
	{}

	~FromTripMatcher()
	{}

	virtual std::string GetMessageCategoryStr() const override { return sk_ValueCat; }

protected:
	virtual Json::Value& GetJsonMsg(Json::Value& outJson) const override;

};

class FromPayment : public Decent::Net::SmartMessages
{
public:
	static constexpr char const sk_ValueCat[] = "FromPayment";

public:
	FromPayment() :
		SmartMessages()
	{}

	FromPayment(const Json::Value& msg) :
		SmartMessages(msg, sk_ValueCat)
	{}

	~FromPayment()
	{}

	virtual std::string GetMessageCategoryStr() const override { return sk_ValueCat; }

protected:
	virtual Json::Value& GetJsonMsg(Json::Value& outJson) const override;

};
