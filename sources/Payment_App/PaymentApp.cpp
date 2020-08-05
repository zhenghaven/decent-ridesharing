#include "PaymentApp.h"

#include <DecentApi/Common/SGX/RuntimeError.h>

#include "../Common_App/RequestCategory.h"

#include "Enclave_u.h"

using namespace RideShare;

bool PaymentApp::ProcessMsgFromTripMatcher(Decent::Net::ConnectionBase& connection)
{
	int retValue = false;
	sgx_status_t enclaveRet = SGX_SUCCESS;

	enclaveRet = ecall_ride_share_pay_from_trip_matcher(GetEnclaveId(), &retValue, &connection);
	DECENT_CHECK_SGX_STATUS_ERROR(enclaveRet, ecall_ride_share_pay_from_trip_matcher);

	return retValue;
}

bool PaymentApp::ProcessSmartMessage(const std::string& category, Decent::Net::ConnectionBase& connection, Decent::Net::ConnectionBase*& freeHeldCnt)
{
	if (category == RequestCategory::sk_fromTripMatcher)
	{
		return ProcessMsgFromTripMatcher(connection);
	}
	else
	{
		return RideShareApp::ProcessSmartMessage(category, connection, freeHeldCnt);
	}
}
