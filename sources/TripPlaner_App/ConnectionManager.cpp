#include <DecentApi/Common/Net/ConnectionBase.h>

#include "../Common_App/ConnectionManager.h"
#include "../Common_App/RequestCategory.h"

using namespace RideShare;
using namespace Decent::Net;

extern "C" void* ocall_ride_share_cnt_mgr_get_pas_mgm()
{
	return ConnectionManager::GetConnection2PassengerMgm(RequestCategory::sk_fromTripPlaner).release();
}

extern "C" void* ocall_ride_share_cnt_mgr_get_billing()
{
	return ConnectionManager::GetConnection2Billing(RequestCategory::sk_fromTripPlaner).release();
}
