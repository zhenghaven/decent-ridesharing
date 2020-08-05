#include <string>
#include <memory>
#include <iostream>

#include <tclap/CmdLine.h>
#include <boost/filesystem.hpp>

#include <sgx_quote.h>

#include <DecentApi/CommonApp/Common.h>

#include <DecentApi/CommonApp/Net/SmartServer.h>
#include <DecentApi/CommonApp/Net/TCPServer.h>
#include <DecentApi/CommonApp/Net/TCPConnection.h>
#include <DecentApi/CommonApp/Tools/DiskFile.h>
#include <DecentApi/CommonApp/Tools/FileSystemUtil.h>
#include <DecentApi/CommonApp/Threading/MainThreadAsynWorker.h>

#include <DecentApi/Common/Common.h>
#include <DecentApi/Common/MbedTls/Initializer.h>
#include <DecentApi/Common/Ra/RequestCategory.h>
#include <DecentApi/Common/Ra/WhiteList/WhiteList.h>

#include <DecentApi/DecentAppApp/DecentAppConfig.h>

#include "../Common/AppNames.h"
#include "../Common_App/ConnectionManager.h"

#include "BillingApp.h"

using namespace RideShare;
using namespace Decent;
using namespace Decent::Net;
using namespace Decent::Tools;
using namespace Decent::AppConfig;

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
	auto mbedInit = Decent::MbedTlsObj::Initializer::Init();

	//------- Construct main thread worker at very first:
	std::shared_ptr<Threading::MainThreadAsynWorker> mainThreadWorker = std::make_shared<Threading::MainThreadAsynWorker>();

	//------- Setup Smart Server:
	Net::SmartServer smartServer(mainThreadWorker);

	std::cout << "================ Billing Services ================" << std::endl;

	TCLAP::CmdLine cmd("Billing Services", ' ', "ver", true);

	TCLAP::ValueArg<std::string> configPathArg("c", "config", "Path to the configuration file.", false, "Config.json", "String");
	TCLAP::ValueArg<std::string> wlKeyArg("w", "wl-key", "Key for the loaded whitelist.", false, "WhiteListKey", "String");
	TCLAP::SwitchArg isSendWlArg("s", "not-send-wl", "Do not send whitelist to Decent Server.", true);
	cmd.add(configPathArg);
	cmd.add(wlKeyArg);
	cmd.add(isSendWlArg);

	cmd.parse(argc, argv);

	const size_t numListenThread = 5;

	//------- Read configuration file:
	std::unique_ptr<DecentAppConfig> configMgr;
	try
	{
		std::string configJsonStr;
		DiskFile file(configPathArg.getValue(), FileBase::Mode::Read, true);
		configJsonStr.resize(file.GetFileSize());
		file.ReadBlockExactSize(configJsonStr);

		configMgr = std::make_unique<DecentAppConfig>(configJsonStr);
	}
	catch (const std::exception& e)
	{
		PRINT_W("Failed to load configuration file. Error Msg: %s", e.what());
		return -1;
	}

	//------- Read Decent Server Configuration:
	uint32_t serverIp = 0;
	uint16_t serverPort = 0;
	try
	{
		const auto& decentServerConfig = configMgr->GetEnclaveList().GetItem(Ra::WhiteList::sk_nameDecentServer);

		serverIp = Net::TCPConnection::GetIpAddressFromStr(decentServerConfig.GetAddr());
		serverPort = decentServerConfig.GetPort();
	}
	catch (const std::exception& e)
	{
		PRINT_W("Failed to read Decent Server configuration. Error Msg: %s", e.what());
		return -1;
	}

	//------- Read self configuration:
	uint32_t selfIp = 0;
	uint16_t selfPort = 0;
	uint64_t selfFullAddr = 0;
	std::string selfAddr;
	try
	{
		const auto& selfItem = configMgr->GetEnclaveList().GetItem(AppNames::sk_billing);

		selfAddr = selfItem.GetAddr();
		selfIp = TCPConnection::GetIpAddressFromStr(selfItem.GetAddr());
		selfPort = selfItem.GetPort();

		selfFullAddr = TCPConnection::CombineIpAndPort(selfIp, selfPort);
	}
	catch (const std::exception& e)
	{
		PRINT_W("Failed to read self configuration. Error Msg: %s", e.what());
		return -1;
	}

	//------- Setup connection manager:
	ConnectionManager::SetEnclaveList(configMgr->GetEnclaveList());

	std::unique_ptr<Net::ConnectionBase> serverCon;
	//------- Send loaded white list to Decent Server when needed.
	try
	{
		if (isSendWlArg.getValue())
		{
			serverCon = std::make_unique<TCPConnection>(serverIp, serverPort);
			serverCon->SendContainer(Ra::RequestCategory::sk_loadWhiteList);
			serverCon->SendContainer(wlKeyArg.getValue());
			serverCon->SendContainer(configMgr->GetEnclaveList().GetLoadedWhiteListStr());
			char ackMsg[] = "ACK";
			serverCon->RecvRawAll(&ackMsg, sizeof(ackMsg));
		}
	}
	catch (const std::exception& e)
	{
		PRINT_W("Failed to send loaded white list to Decent Server. Error Msg: %s", e.what());
		return -1;
	}

	//------- Setup TCP server:
	std::unique_ptr<Server> server;
	try
	{
		server = std::make_unique<Net::TCPServer>(selfIp, selfPort);
	}
	catch (const std::exception& e)
	{
		PRINT_W("Failed to start TCP server. Error Message: %s", e.what());
		return -1;
	}

	//------- Setup Enclave:
	std::shared_ptr<Billing> enclave;
	try
	{
		serverCon = std::make_unique<TCPConnection>(serverIp, serverPort);

		boost::filesystem::path tokenPath = GetKnownFolderPath(KnownFolderType::LocalAppDataEnclave).append(TOKEN_FILENAME);

		enclave = std::make_shared<Billing>(
			ENCLAVE_FILENAME, tokenPath, wlKeyArg.getValue(), *serverCon,
			"Billing Pay Info" + selfAddr + ":" + std::to_string(selfPort));

		smartServer.AddServer(server, enclave, nullptr, numListenThread, 0);
	}
	catch (const std::exception& e)
	{
		PRINT_W("Failed to start enclave program! Error Msg:\n%s", e.what());
		return -1;
	}

	//------- keep running until an interrupt signal (Ctrl + C) is received.
	mainThreadWorker->UpdateUntilInterrupt();

	//------- Exit...
	enclave.reset();
	smartServer.Terminate();

	PRINT_I("Exit.");
	return 0;
}
