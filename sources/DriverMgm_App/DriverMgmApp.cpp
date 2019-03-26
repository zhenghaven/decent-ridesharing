#include "DriverMgmApp.h"

#include <DecentApi/Common/SGX/RuntimeError.h>

#include "../Common_App/RideSharingMessages.h"

#include "Enclave_u.h"

using namespace RideShare;

bool DriverMgm::ProcessMsgFromDriver(Decent::Net::Connection & connection)
{
	int retValue = false;
	sgx_status_t enclaveRet = SGX_SUCCESS;

	enclaveRet = ecall_ride_share_dm_from_dri(GetEnclaveId(), &retValue, &connection);
	DECENT_CHECK_SGX_STATUS_ERROR(enclaveRet, ecall_ride_share_dm_from_dri);

	return retValue;
}

bool DriverMgm::ProcessMsgFromTripMatcher(Decent::Net::Connection & connection)
{
	int retValue = false;
	sgx_status_t enclaveRet = SGX_SUCCESS;

	enclaveRet = ecall_ride_share_dm_from_trip_matcher(GetEnclaveId(), &retValue, &connection);
	DECENT_CHECK_SGX_STATUS_ERROR(enclaveRet, ecall_ride_share_dm_from_trip_matcher);

	return retValue;
}

bool DriverMgm::ProcessMsgFromPayment(Decent::Net::Connection & connection)
{
	int retValue = false;
	sgx_status_t enclaveRet = SGX_SUCCESS;

	enclaveRet = ecall_ride_share_dm_from_payment(GetEnclaveId(), &retValue, &connection);
	DECENT_CHECK_SGX_STATUS_ERROR(enclaveRet, ecall_ride_share_dm_from_payment);

	return retValue;
}

bool DriverMgm::ProcessSmartMessage(const std::string & category, const Json::Value & jsonMsg, Decent::Net::Connection & connection)
{
	if (category == FromDriver::sk_ValueCat)
	{
		return ProcessMsgFromDriver(connection);
	}
	else if (category == FromTripMatcher::sk_ValueCat)
	{
		return ProcessMsgFromTripMatcher(connection);
	}
	else if (category == FromPayment::sk_ValueCat)
	{
		return ProcessMsgFromPayment(connection);
	}
	else
	{
		return Decent::RaSgx::DecentApp::ProcessSmartMessage(category, jsonMsg, connection);
	}
}
