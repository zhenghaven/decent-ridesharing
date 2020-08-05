#include "RideShareApp.h"

#ifndef DECENT_PURE_CLIENT

#include <sgx_error.h>
#include <DecentApi/Common/SGX/RuntimeError.h>

using namespace RideShare;
using namespace Decent::Net;

extern "C" sgx_status_t ecall_ride_share_init(sgx_enclave_id_t eid, const char* pay_info);

RideShareApp::RideShareApp(const std::string& enclavePath, const std::string& tokenPath, const std::string& wListKey, Decent::Net::ConnectionBase& serverConn, const std::string& opPayInfo) :
	Decent::RaSgx::DecentApp(enclavePath, tokenPath, wListKey, serverConn)
{
	InitEnclave(opPayInfo);
}

RideShareApp::RideShareApp(const fs::path& enclavePath, const fs::path& tokenPath, const std::string& wListKey, Decent::Net::ConnectionBase& serverConn, const std::string& opPayInfo) :
	Decent::RaSgx::DecentApp(enclavePath, tokenPath, wListKey, serverConn)
{
	InitEnclave(opPayInfo);
}

RideShareApp::RideShareApp(const std::string& enclavePath, const std::string& tokenPath,
	const size_t numTWorker, const size_t numUWorker, const size_t retryFallback, const size_t retrySleep,
	const std::string& wListKey, Decent::Net::ConnectionBase& serverConn,
	const std::string& opPayInfo) :
	Decent::RaSgx::DecentApp(enclavePath, tokenPath, 
		numTWorker, numUWorker, retryFallback, retrySleep, 
		wListKey, serverConn)
{
	InitEnclave(opPayInfo);
}

RideShareApp::RideShareApp(const fs::path& enclavePath, const fs::path& tokenPath,
	const size_t numTWorker, const size_t numUWorker, const size_t retryFallback, const size_t retrySleep,
	const std::string& wListKey, Decent::Net::ConnectionBase& serverConn,
	const std::string& opPayInfo) :
	Decent::RaSgx::DecentApp(enclavePath, tokenPath, 
		numTWorker, numUWorker, retryFallback, retrySleep, 
		wListKey, serverConn)
{
	InitEnclave(opPayInfo);
}

void RideShare::RideShareApp::InitEnclave(const std::string & opPayInfo)
{
	sgx_status_t enclaveRet = ecall_ride_share_init(GetEnclaveId(), opPayInfo.c_str());
	DECENT_CHECK_SGX_STATUS_ERROR(enclaveRet, ecall_ride_share_init);
}

#endif // !DECENT_PURE_CLIENT
