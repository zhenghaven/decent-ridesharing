#pragma once

#include <memory>

namespace Decent
{
	namespace Net
	{
		class Connection;
		class SmartMessages;
	}

	namespace Tools
	{
		class ConfigManager;
	}
}

namespace RideShare
{
	namespace ConnectionManager
	{
		void SetConfigManager(const Decent::Tools::ConfigManager& mgrRef);

		std::unique_ptr<Decent::Net::Connection> GetConnection2PassengerMgm(const Decent::Net::SmartMessages& hsMsg);
		std::unique_ptr<Decent::Net::Connection> GetConnection2TripPlanner(const Decent::Net::SmartMessages& hsMsg);
		std::unique_ptr<Decent::Net::Connection> GetConnection2TripMatcher(const Decent::Net::SmartMessages& hsMsg);
		std::unique_ptr<Decent::Net::Connection> GetConnection2Billing(const Decent::Net::SmartMessages& hsMsg);
		std::unique_ptr<Decent::Net::Connection> GetConnection2Payment(const Decent::Net::SmartMessages& hsMsg);
		std::unique_ptr<Decent::Net::Connection> GetConnection2DriverMgm(const Decent::Net::SmartMessages& hsMsg);
	}
}
