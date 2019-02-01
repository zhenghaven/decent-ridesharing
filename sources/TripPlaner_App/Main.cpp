#include <cstdio>
#include <cstring>

#include <string>
#include <memory>
#include <iostream>

#include <tclap/CmdLine.h>
#include <boost/asio/ip/address_v4.hpp>
#include <json/json.h>
#include <sgx_quote.h>

#include <DecentApi/CommonApp/Common.h>

#include <DecentApi/CommonApp/Net/SmartMessages.h>
#include <DecentApi/CommonApp/Net/TCPConnection.h>
#include <DecentApi/CommonApp/Net/TCPServer.h>
#include <DecentApi/CommonApp/Net/SmartServer.h>
#include <DecentApi/CommonApp/Ra/WhiteList/Requester.h>

#include <DecentApi/Common/Tools/JsonTools.h>

#include "../Common_App/RideSharingMessages.h"

#include "TripPlanerApp.h"

using namespace Decent;
using namespace Decent::Tools;

/**
 * \brief	Main entry-point for this application
 *
 * \param	argc	The number of command-line arguments provided.
 * \param	argv	An array of command-line argument strings.
 *
 * \return	Exit-code for the process - 0 for success, else an error code.
 */
int main(int argc, char ** argv)
{
	std::cout << "================ Trip Planer ================" << std::endl;

	TCLAP::CmdLine cmd("Decent Remote Attestation", ' ', "ver", true);

#ifndef DEBUG
	TCLAP::ValueArg<uint16_t>  argServerPort("p", "port", "Port number for on-coming local connection.", true, 0, "[0-65535]");
	cmd.add(argServerPort);
#else
	TCLAP::ValueArg<int> testOpt("t", "test-opt", "Test Option Number", false, 0, "A single digit number.");
	cmd.add(testOpt);
#endif

	cmd.parse(argc, argv);

#ifndef DEBUG
	std::string serverAddr = "127.0.0.1";
	uint16_t serverPort = argServerPort.getValue();
	std::string localAddr = "DecentServerLocal";
#else
	uint16_t rootServerPort = 57755U;

	std::string serverAddr = "127.0.0.1";
	uint16_t serverPort = rootServerPort + testOpt.getValue();
	std::string localAddr = "DecentServerLocal";
#endif

	uint32_t serverIp = boost::asio::ip::address_v4::from_string(serverAddr).to_uint();
	//Net::SmartServer smartServer;

	std::unique_ptr<Net::Connection> serverCon = std::make_unique<Net::TCPConnection>(serverIp, serverPort);

	const Ra::WhiteList::Requester& wlReq = Ra::WhiteList::Requester::Get();
	wlReq.SendRequest(*serverCon);

	serverCon = std::make_unique<Net::TCPConnection>(serverIp, serverPort);

	std::shared_ptr<TripPlanerApp> enclave(
		std::make_shared<TripPlanerApp>(
			ENCLAVE_FILENAME, KnownFolderType::LocalAppDataEnclave, TOKEN_FILENAME, wlReq.GetKey(), *serverCon));

	

	printf("Exit ...\n");
	return 0;
}
