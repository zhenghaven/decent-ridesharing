#pragma once

#include <memory>
#include <string>

namespace Decent
{
	namespace Net
	{
		class ConnectionBase;
	}

	namespace AppConfig
	{
		class EnclaveList;
	}
}

namespace RideShare
{
	namespace ConnectionManager
	{
		void SetEnclaveList(const Decent::AppConfig::EnclaveList& list);

		std::unique_ptr<Decent::Net::ConnectionBase> GetConnection2PassengerMgm(const std::string& category);
		std::unique_ptr<Decent::Net::ConnectionBase> GetConnection2TripPlanner(const std::string& category);
		std::unique_ptr<Decent::Net::ConnectionBase> GetConnection2TripMatcher(const std::string& category);
		std::unique_ptr<Decent::Net::ConnectionBase> GetConnection2Billing(const std::string& category);
		std::unique_ptr<Decent::Net::ConnectionBase> GetConnection2Payment(const std::string& category);
		std::unique_ptr<Decent::Net::ConnectionBase> GetConnection2DriverMgm(const std::string& category);
	}
}
