#include "TripMatcherApp.h"

#include <DecentApi/CommonApp/SGX/EnclaveRuntimeException.h>

#include "../Common_App/RideSharingMessages.h"

#include "Enclave_u.h"

using namespace RideShare;

bool TripMatcher::ProcessMsgFromPassenger(Decent::Net::Connection & connection)
{
	int retValue = false;
	sgx_status_t enclaveRet = SGX_SUCCESS;

	enclaveRet = ecall_ride_share_tm_from_pas(GetEnclaveId(), &retValue, &connection);
	CHECK_SGX_ENCLAVE_RUNTIME_EXCEPTION(enclaveRet, ecall_ride_share_tm_from_pas);

	return retValue;
}

bool TripMatcher::ProcessMsgFromDriver(Decent::Net::Connection & connection)
{
	int retValue = false;
	sgx_status_t enclaveRet = SGX_SUCCESS;

	enclaveRet = ecall_ride_share_tm_from_dri(GetEnclaveId(), &retValue, &connection);
	CHECK_SGX_ENCLAVE_RUNTIME_EXCEPTION(enclaveRet, ecall_ride_share_tm_from_dri);

	return retValue;
}

bool TripMatcher::ProcessSmartMessage(const std::string & category, const Json::Value & jsonMsg, Decent::Net::Connection & connection)
{
	if (category == FromPassenger::sk_ValueCat)
	{
		return ProcessMsgFromPassenger(connection);
	}
	else if (category == FromDriver::sk_ValueCat)
	{
		return ProcessMsgFromDriver(connection);
	}
	else
	{
		return Decent::RaSgx::DecentApp::ProcessSmartMessage(category, jsonMsg, connection);
	}
}
