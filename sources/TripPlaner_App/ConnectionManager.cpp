#include <DecentApi/CommonApp/Net/Connection.h>

#include "../Common_App/ConnectionManager.h"
#include "../Common_App/RideSharingMessages.h"

using namespace RideShare;
using namespace Decent::Net;

extern "C" void ocall_ride_share_cnt_mgr_get_pas_mgm(void** out_cnt_ptr)
{
	if (!out_cnt_ptr)
	{
		return;
	}
	std::unique_ptr<Connection> cnt = ConnectionManager::GetConnection2PassengerMgm(FromTripPlaner());

	*out_cnt_ptr = cnt.release();
}

extern "C" void ocall_ride_share_cnt_mgr_get_billing(void** out_cnt_ptr)
{
	if (!out_cnt_ptr)
	{
		return;
	}
	std::unique_ptr<Connection> cnt = ConnectionManager::GetConnection2Billing(FromTripPlaner());

	*out_cnt_ptr = cnt.release();
}

extern "C" void ocall_ride_share_cnt_mgr_get_trip_matcher(void** out_cnt_ptr, uint32_t* out_ip, uint16_t* out_port)
{
	if (!out_cnt_ptr)
	{
		return;
	}
	std::unique_ptr<Connection> cnt = ConnectionManager::GetConnection2TripMatcher(FromTripPlaner(), out_ip, out_port);

	*out_cnt_ptr = cnt.release();
}
