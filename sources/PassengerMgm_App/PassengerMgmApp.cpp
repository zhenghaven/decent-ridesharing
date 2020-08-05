#include "PassengerMgmApp.h"

#include <DecentApi/Common/SGX/RuntimeError.h>

#include "../Common_App/RequestCategory.h"

#include "Enclave_u.h"

using namespace RideShare;

bool PassengerMgm::ProcessMsgFromPassenger(Decent::Net::ConnectionBase& connection)
{
	int retValue = false;
	sgx_status_t enclaveRet = SGX_SUCCESS;

	enclaveRet = ecall_ride_share_pm_from_pas(GetEnclaveId(), &retValue, &connection);
	DECENT_CHECK_SGX_STATUS_ERROR(enclaveRet, ecall_ride_share_pm_from_pas);

	return retValue;
}

bool PassengerMgm::ProcessMsgFromTripPlanner(Decent::Net::ConnectionBase& connection)
{
	int retValue = false;
	sgx_status_t enclaveRet = SGX_SUCCESS;

	enclaveRet = ecall_ride_share_pm_from_trip_planner(GetEnclaveId(), &retValue, &connection);
	DECENT_CHECK_SGX_STATUS_ERROR(enclaveRet, ecall_ride_share_pm_from_trip_planner);

	return retValue;
}

bool PassengerMgm::ProcessMsgFromPayment(Decent::Net::ConnectionBase& connection)
{
	int retValue = false;
	sgx_status_t enclaveRet = SGX_SUCCESS;

	enclaveRet = ecall_ride_share_pm_from_payment(GetEnclaveId(), &retValue, &connection);
	DECENT_CHECK_SGX_STATUS_ERROR(enclaveRet, ecall_ride_share_pm_from_trip_planner);

	return retValue;
}

bool PassengerMgm::ProcessSmartMessage(const std::string& category, Decent::Net::ConnectionBase& connection, Decent::Net::ConnectionBase*& freeHeldCnt)
{
	if (category == RequestCategory::sk_fromPassenger)
	{
		return ProcessMsgFromPassenger(connection);
	}
	else if (category == RequestCategory::sk_fromTripPlaner)
	{
		return ProcessMsgFromTripPlanner(connection);
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
