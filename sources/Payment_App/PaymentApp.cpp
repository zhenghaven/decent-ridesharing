#include "PaymentApp.h"

#include <DecentApi/CommonApp/SGX/EnclaveRuntimeException.h>

#include "../Common_App/RideSharingMessages.h"

#include "Enclave_u.h"

using namespace RideShare;

bool PaymentApp::ProcessMsgFromTripMatcher(Decent::Net::Connection & connection)
{
	int retValue = false;
	sgx_status_t enclaveRet = SGX_SUCCESS;

	enclaveRet = ecall_ride_share_pay_from_trip_matcher(GetEnclaveId(), &retValue, &connection);
	CHECK_SGX_ENCLAVE_RUNTIME_EXCEPTION(enclaveRet, ecall_ride_share_pay_from_trip_matcher);

	return retValue;
}

bool PaymentApp::ProcessSmartMessage(const std::string & category, const Json::Value & jsonMsg, Decent::Net::Connection & connection)
{
	if (category == FromTripMatcher::sk_ValueCat)
	{
		return ProcessMsgFromTripMatcher(connection);
	}
	else
	{
		return Decent::RaSgx::DecentApp::ProcessSmartMessage(category, jsonMsg, connection);
	}
}
