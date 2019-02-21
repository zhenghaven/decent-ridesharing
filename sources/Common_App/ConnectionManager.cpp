#include "ConnectionManager.h"

#include <boost/asio/ip/address_v4.hpp>

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

	static inline std::unique_ptr<Connection> InternalGetConnection(const ConfigItem& configItem, const SmartMessages& hsMsg, uint32_t* outIpAddr, uint16_t* outPort)
	{
		uint32_t ip = boost::asio::ip::address_v4::from_string(configItem.GetAddr()).to_uint();
		if (outIpAddr)
		{
			*outIpAddr = ip;
		}
		if (outPort)
		{
			*outPort = configItem.GetPort();
		}

		try
		{
			std::unique_ptr<Connection> connection = std::make_unique<TCPConnection>(ip, configItem.GetPort());

			if (connection)
			{
				connection->SendPack(hsMsg);
			}
			return std::move(connection);
		}
		catch (const std::exception& e)
		{
			LOGW("Failed to establish connection. (Err Msg: %s)", e.what());
			return nullptr;
		}
	}
}

void ConnectionManager::SetConfigManager(const ConfigManager & mgrRef)
{
	gsk_configMgrPtr = &mgrRef;
}

std::unique_ptr<Connection> ConnectionManager::GetConnection2PassengerMgm(const SmartMessages& hsMsg, uint32_t* outIpAddr, uint16_t* outPort)
{
	const ConfigItem& pasMgmItem = gsk_configMgrPtr->GetItem(AppNames::sk_passengerMgm);

	return InternalGetConnection(pasMgmItem, hsMsg, outIpAddr, outPort);
}

std::unique_ptr<Connection> ConnectionManager::GetConnection2TripPlanner(const SmartMessages& hsMsg, uint32_t* outIpAddr, uint16_t* outPort)
{
	const ConfigItem& pasMgmItem = gsk_configMgrPtr->GetItem(AppNames::sk_tripPlanner);

	return InternalGetConnection(pasMgmItem, hsMsg, outIpAddr, outPort);
}

std::unique_ptr<Connection> ConnectionManager::GetConnection2TripMatcher(const SmartMessages& hsMsg, uint32_t* outIpAddr, uint16_t* outPort)
{
	const ConfigItem& pasMgmItem = gsk_configMgrPtr->GetItem(AppNames::sk_tripMatcher);

	return InternalGetConnection(pasMgmItem, hsMsg, outIpAddr, outPort);
}

std::unique_ptr<Connection> ConnectionManager::GetConnection2Billing(const SmartMessages& hsMsg, uint32_t* outIpAddr, uint16_t* outPort)
{
	const ConfigItem& pasMgmItem = gsk_configMgrPtr->GetItem(AppNames::sk_billing);

	return InternalGetConnection(pasMgmItem, hsMsg, outIpAddr, outPort);
}

std::unique_ptr<Connection> ConnectionManager::GetConnection2Payment(const SmartMessages& hsMsg, uint32_t* outIpAddr, uint16_t* outPort)
{
	const ConfigItem& pasMgmItem = gsk_configMgrPtr->GetItem(AppNames::sk_payment);

	return InternalGetConnection(pasMgmItem, hsMsg, outIpAddr, outPort);
}

std::unique_ptr<Connection> ConnectionManager::GetConnection2DriverMgm(const SmartMessages& hsMsg, uint32_t* outIpAddr, uint16_t* outPort)
{
	const ConfigItem& pasMgmItem = gsk_configMgrPtr->GetItem(AppNames::sk_driverMgm);

	return InternalGetConnection(pasMgmItem, hsMsg, outIpAddr, outPort);
}

extern "C" void ocall_ride_share_cnt_mgr_close_cnt(void* cnt_ptr)
{
	delete static_cast<Connection*>(cnt_ptr);
}
