#include <DecentApi/Common/Net/ConnectionBase.h>

#include "../Common_App/ConnectionManager.h"
#include "../Common_App/RequestCategory.h"

using namespace RideShare;
using namespace Decent::Net;

extern "C" void* ocall_ride_share_cnt_mgr_get_dri_mgm()
{
	return ConnectionManager::GetConnection2DriverMgm(RequestCategory::sk_fromTripMatcher).release();
}

extern "C" void* ocall_ride_share_cnt_mgr_get_payment()
{
	return ConnectionManager::GetConnection2Payment(RequestCategory::sk_fromTripMatcher).release();
}
