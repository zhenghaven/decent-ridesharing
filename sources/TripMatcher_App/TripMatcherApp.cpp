#include "TripMatcherApp.h"

#include <DecentApi/Common/SGX/RuntimeError.h>

#include "../Common_App/RequestCategory.h"

#include "Enclave_u.h"

using namespace RideShare;

bool TripMatcher::ProcessMsgFromPassenger(Decent::Net::ConnectionBase & connection)
{
	int retValue = false;
	sgx_status_t enclaveRet = SGX_SUCCESS;

	enclaveRet = ecall_ride_share_tm_from_pas(GetEnclaveId(), &retValue, &connection);
	DECENT_CHECK_SGX_STATUS_ERROR(enclaveRet, ecall_ride_share_tm_from_pas);

	return retValue;
}

bool TripMatcher::ProcessMsgFromDriver(Decent::Net::ConnectionBase& connection)
{
	int retValue = false;
	sgx_status_t enclaveRet = SGX_SUCCESS;

	enclaveRet = ecall_ride_share_tm_from_dri(GetEnclaveId(), &retValue, &connection);
	DECENT_CHECK_SGX_STATUS_ERROR(enclaveRet, ecall_ride_share_tm_from_dri);

	return retValue;
}

bool TripMatcher::ProcessSmartMessage(const std::string& category, Decent::Net::ConnectionBase& connection, Decent::Net::ConnectionBase*& freeHeldCnt)
{
	if (category == RequestCategory::sk_fromPassenger)
	{
		return ProcessMsgFromPassenger(connection);
	}
	else if (category == RequestCategory::sk_fromDriver)
	{
		return ProcessMsgFromDriver(connection);
	}
	else
	{
		return RideShareApp::ProcessSmartMessage(category, connection, freeHeldCnt);
	}
}
