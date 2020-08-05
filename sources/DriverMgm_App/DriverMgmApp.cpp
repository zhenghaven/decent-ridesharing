#include "DriverMgmApp.h"

#include <DecentApi/Common/SGX/RuntimeError.h>

#include "../Common_App/RequestCategory.h"

#include "Enclave_u.h"

using namespace RideShare;

bool DriverMgm::ProcessMsgFromDriver(Decent::Net::ConnectionBase& connection)
{
	int retValue = false;
	sgx_status_t enclaveRet = SGX_SUCCESS;

	enclaveRet = ecall_ride_share_dm_from_dri(GetEnclaveId(), &retValue, &connection);
	DECENT_CHECK_SGX_STATUS_ERROR(enclaveRet, ecall_ride_share_dm_from_dri);

	return retValue;
}

bool DriverMgm::ProcessMsgFromTripMatcher(Decent::Net::ConnectionBase& connection)
{
	int retValue = false;
	sgx_status_t enclaveRet = SGX_SUCCESS;

	enclaveRet = ecall_ride_share_dm_from_trip_matcher(GetEnclaveId(), &retValue, &connection);
	DECENT_CHECK_SGX_STATUS_ERROR(enclaveRet, ecall_ride_share_dm_from_trip_matcher);

	return retValue;
}

bool DriverMgm::ProcessMsgFromPayment(Decent::Net::ConnectionBase& connection)
{
	int retValue = false;
	sgx_status_t enclaveRet = SGX_SUCCESS;

	enclaveRet = ecall_ride_share_dm_from_payment(GetEnclaveId(), &retValue, &connection);
	DECENT_CHECK_SGX_STATUS_ERROR(enclaveRet, ecall_ride_share_dm_from_payment);

	return retValue;
}

bool DriverMgm::ProcessSmartMessage(const std::string& category, Decent::Net::ConnectionBase& connection, Decent::Net::ConnectionBase*& freeHeldCnt)
{
	if (category == RequestCategory::sk_fromDriver)
	{
		return ProcessMsgFromDriver(connection);
	}
	else if (category == RequestCategory::sk_fromTripMatcher)
	{
		return ProcessMsgFromTripMatcher(connection);
	}
	else if (category == RequestCategory::sk_fromPayment)
	{
		return ProcessMsgFromPayment(connection);
	}
	else
	{
		return RideShareApp::ProcessSmartMessage(category, connection, freeHeldCnt);
	}
}
