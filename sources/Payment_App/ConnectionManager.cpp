#include <DecentApi/CommonApp/Net/Connection.h>

#include "../Common_App/ConnectionManager.h"
#include "../Common_App/RideSharingMessages.h"

using namespace RideShare;
using namespace Decent::Net;

extern "C" void* ocall_ride_share_cnt_mgr_get_pas_mgm()
{
	return ConnectionManager::GetConnection2PassengerMgm(FromPayment()).release();
}

extern "C" void* ocall_ride_share_cnt_mgr_get_dri_mgm()
{
	return ConnectionManager::GetConnection2DriverMgm(FromPayment()).release();
}
