#include "ConnectionManager.h"

#include <DecentApi/Common/Common.h>
#include <DecentApi/CommonApp/Net/TCPConnection.h>
#include <DecentApi/CommonApp/Tools/ConfigManager.h>

#include "../Common/AppNames.h"

using namespace RideShare;
using namespace Decent::Tools;
using namespace Decent::Net;

namespace
{
	static const ConfigManager* gsk_configMgrPtr = nullptr;

	static inline std::unique_ptr<Connection> InternalGetConnection(const ConfigItem& configItem, const SmartMessages& hsMsg)
	{
		try
		{
			std::unique_ptr<Connection> connection = std::make_unique<TCPConnection>(configItem.GetAddr(), configItem.GetPort());

			if (connection)
			{
				connection->SendPack(hsMsg);
			}
			return std::move(connection);
		}
		catch (const std::exception& e)
		{
			const char* msgStr = e.what();
			LOGW("Failed to establish connection. (Err Msg: %s)", msgStr);
			return nullptr;
		}
	}
}

void ConnectionManager::SetConfigManager(const ConfigManager & mgrRef)
{
	gsk_configMgrPtr = &mgrRef;
}

std::unique_ptr<Connection> ConnectionManager::GetConnection2PassengerMgm(const SmartMessages& hsMsg)
{
	const ConfigItem& pasMgmItem = gsk_configMgrPtr->GetItem(AppNames::sk_passengerMgm);

	return InternalGetConnection(pasMgmItem, hsMsg);
}

std::unique_ptr<Connection> ConnectionManager::GetConnection2TripPlanner(const SmartMessages& hsMsg)
{
	const ConfigItem& pasMgmItem = gsk_configMgrPtr->GetItem(AppNames::sk_tripPlanner);

	return InternalGetConnection(pasMgmItem, hsMsg);
}

std::unique_ptr<Connection> ConnectionManager::GetConnection2TripMatcher(const SmartMessages& hsMsg)
{
	const ConfigItem& pasMgmItem = gsk_configMgrPtr->GetItem(AppNames::sk_tripMatcher);

	return InternalGetConnection(pasMgmItem, hsMsg);
}

std::unique_ptr<Connection> ConnectionManager::GetConnection2Billing(const SmartMessages& hsMsg)
{
	const ConfigItem& pasMgmItem = gsk_configMgrPtr->GetItem(AppNames::sk_billing);

	return InternalGetConnection(pasMgmItem, hsMsg);
}

std::unique_ptr<Connection> ConnectionManager::GetConnection2Payment(const SmartMessages& hsMsg)
{
	const ConfigItem& pasMgmItem = gsk_configMgrPtr->GetItem(AppNames::sk_payment);

	return InternalGetConnection(pasMgmItem, hsMsg);
}

std::unique_ptr<Connection> ConnectionManager::GetConnection2DriverMgm(const SmartMessages& hsMsg)
{
	const ConfigItem& pasMgmItem = gsk_configMgrPtr->GetItem(AppNames::sk_driverMgm);

	return InternalGetConnection(pasMgmItem, hsMsg);
}
