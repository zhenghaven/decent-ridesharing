#include "BillingApp.h"

#include <DecentApi/CommonApp/SGX/EnclaveRuntimeException.h>

#include "../Common_App/RideSharingMessages.h"

#include "Enclave_u.h"

bool Billing::ProcessMsgFromTripPlanner(Decent::Net::Connection & connection)
{
	int retValue = false;
	sgx_status_t enclaveRet = SGX_SUCCESS;

	enclaveRet = ecall_ride_share_pm_from_trip_planner(GetEnclaveId(), &retValue, &connection);
	CHECK_SGX_ENCLAVE_RUNTIME_EXCEPTION(enclaveRet, ecall_ride_share_pm_from_trip_planner);

	return retValue;
}

bool Billing::ProcessSmartMessage(const std::string & category, const Json::Value & jsonMsg, Decent::Net::Connection & connection)
{
	if (category == FromTripPlaner::sk_ValueCat)
	{
		return ProcessMsgFromTripPlanner(connection);
	}
	else
	{
		return Decent::RaSgx::DecentApp::ProcessSmartMessage(category, jsonMsg, connection);
	}
}
