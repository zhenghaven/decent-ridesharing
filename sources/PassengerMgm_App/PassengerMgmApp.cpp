#include "PassengerMgmApp.h"

#include <DecentApi/Common/SGX/RuntimeError.h>

#include "../Common_App/RideSharingMessages.h"

#include "Enclave_u.h"

using namespace RideShare;

bool PassengerMgm::ProcessMsgFromPassenger(Decent::Net::Connection & connection)
{
	int retValue = false;
	sgx_status_t enclaveRet = SGX_SUCCESS;

	enclaveRet = ecall_ride_share_pm_from_pas(GetEnclaveId(), &retValue, &connection);
	DECENT_CHECK_SGX_STATUS_ERROR(enclaveRet, ecall_ride_share_pm_from_pas);

	return retValue;
}

bool PassengerMgm::ProcessMsgFromTripPlanner(Decent::Net::Connection & connection)
{
	int retValue = false;
	sgx_status_t enclaveRet = SGX_SUCCESS;

	enclaveRet = ecall_ride_share_pm_from_trip_planner(GetEnclaveId(), &retValue, &connection);
	DECENT_CHECK_SGX_STATUS_ERROR(enclaveRet, ecall_ride_share_pm_from_trip_planner);

	return retValue;
}

bool PassengerMgm::ProcessMsgFromPayment(Decent::Net::Connection & connection)
{
	int retValue = false;
	sgx_status_t enclaveRet = SGX_SUCCESS;

	enclaveRet = ecall_ride_share_pm_from_payment(GetEnclaveId(), &retValue, &connection);
	DECENT_CHECK_SGX_STATUS_ERROR(enclaveRet, ecall_ride_share_pm_from_trip_planner);

	return retValue;
}

bool PassengerMgm::ProcessSmartMessage(const std::string & category, const Json::Value & jsonMsg, Decent::Net::Connection & connection)
{
	if (category == FromPassenger::sk_ValueCat)
	{
		return ProcessMsgFromPassenger(connection);
	}
	else if (category == FromTripPlaner::sk_ValueCat)
	{
		return ProcessMsgFromTripPlanner(connection);
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
