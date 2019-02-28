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
	std::unique_ptr<Connection> cnt = ConnectionManager::GetConnection2PassengerMgm(FromPayment());

	*out_cnt_ptr = cnt.release();
}

extern "C" void ocall_ride_share_cnt_mgr_get_dri_mgm(void** out_cnt_ptr)
{
	if (!out_cnt_ptr)
	{
		return;
	}
	std::unique_ptr<Connection> cnt = ConnectionManager::GetConnection2DriverMgm(FromPayment());

	*out_cnt_ptr = cnt.release();
}
