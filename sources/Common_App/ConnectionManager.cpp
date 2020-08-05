#include "ConnectionManager.h"

#include <DecentApi/Common/Common.h>
#include <DecentApi/CommonApp/Net/TCPConnection.h>
#include <DecentApi/CommonApp/AppConfig/EnclaveList.h>

#include "../Common/AppNames.h"

using namespace RideShare;
using namespace Decent::Tools;
using namespace Decent::Net;

namespace
{
	static const Decent::AppConfig::EnclaveList* gsk_enclaveList = nullptr;

	static inline std::unique_ptr<ConnectionBase> InternalGetConnection(const Decent::AppConfig::EnclaveListItem& enclaveItem, const std::string& category)
	{
		try
		{
			std::unique_ptr<ConnectionBase> connection = std::make_unique<TCPConnection>(enclaveItem.GetAddr(), enclaveItem.GetPort());

			if (connection)
			{
				connection->SendContainer(category);
				return std::move(connection);
			}
			else
			{
				LOGW("Failed to establish connection. (Memory allocation failed).");
				throw std::runtime_error("Memory allocation failed");
			}
		}
		catch (const std::exception& e)
		{
			const char* msgStr = e.what();
			LOGW("Failed to establish connection. (Err Msg: %s)", msgStr);
			throw e;
		}
	}
}

void ConnectionManager::SetEnclaveList(const Decent::AppConfig::EnclaveList& list)
{
	gsk_enclaveList = &list;
}

std::unique_ptr<ConnectionBase> ConnectionManager::GetConnection2PassengerMgm(const std::string& category)
{
	const auto& enclaveItem = gsk_enclaveList->GetItem(AppNames::sk_passengerMgm);

	return InternalGetConnection(enclaveItem, category);
}

std::unique_ptr<ConnectionBase> ConnectionManager::GetConnection2TripPlanner(const std::string& category)
{
	const auto& enclaveItem = gsk_enclaveList->GetItem(AppNames::sk_tripPlanner);

	return InternalGetConnection(enclaveItem, category);
}

std::unique_ptr<ConnectionBase> ConnectionManager::GetConnection2TripMatcher(const std::string& category)
{
	const auto& enclaveItem = gsk_enclaveList->GetItem(AppNames::sk_tripMatcher);

	return InternalGetConnection(enclaveItem, category);
}

std::unique_ptr<ConnectionBase> ConnectionManager::GetConnection2Billing(const std::string& category)
{
	const auto& enclaveItem = gsk_enclaveList->GetItem(AppNames::sk_billing);

	return InternalGetConnection(enclaveItem, category);
}

std::unique_ptr<ConnectionBase> ConnectionManager::GetConnection2Payment(const std::string& category)
{
	const auto& enclaveItem = gsk_enclaveList->GetItem(AppNames::sk_payment);

	return InternalGetConnection(enclaveItem, category);
}

std::unique_ptr<ConnectionBase> ConnectionManager::GetConnection2DriverMgm(const std::string& category)
{
	const auto& enclaveItem = gsk_enclaveList->GetItem(AppNames::sk_driverMgm);

	return InternalGetConnection(enclaveItem, category);
}
