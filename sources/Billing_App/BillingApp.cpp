#include "BillingApp.h"

#include <DecentApi/Common/SGX/RuntimeError.h>

#include "../Common_App/RequestCategory.h"

#include "Enclave_u.h"

using namespace RideShare;

bool Billing::ProcessMsgFromTripPlanner(Decent::Net::ConnectionBase& connection)
{
	int retValue = false;
	sgx_status_t enclaveRet = SGX_SUCCESS;

	enclaveRet = ecall_ride_share_bill_from_trip_planner(GetEnclaveId(), &retValue, &connection);
	DECENT_CHECK_SGX_STATUS_ERROR(enclaveRet, ecall_ride_share_bill_from_trip_planner);

	return retValue;
}

bool Billing::ProcessSmartMessage(const std::string& category, Decent::Net::ConnectionBase& connection, Decent::Net::ConnectionBase*& freeHeldCnt)
{
	if (category == RequestCategory::sk_fromTripPlaner)
	{
		return ProcessMsgFromTripPlanner(connection);
	}
	else
	{
		return RideShareApp::ProcessSmartMessage(category, connection, freeHeldCnt);
	}
}
