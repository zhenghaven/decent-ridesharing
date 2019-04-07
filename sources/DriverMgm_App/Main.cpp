#include <string>
#include <memory>
#include <iostream>

#include <tclap/CmdLine.h>
#include <sgx_quote.h>

#include <DecentApi/CommonApp/Common.h>

#include <DecentApi/CommonApp/Net/SmartMessages.h>
#include <DecentApi/CommonApp/Net/TCPConnection.h>
#include <DecentApi/CommonApp/Net/TCPServer.h>
#include <DecentApi/CommonApp/Net/SmartServer.h>
#include <DecentApi/CommonApp/Ra/Messages.h>
#include <DecentApi/CommonApp/Tools/ConfigManager.h>

#include <DecentApi/Common/Common.h>
#include <DecentApi/Common/Ra/WhiteList/WhiteList.h>

#include "../Common/AppNames.h"
#include "../Common_App/Tools.h"
#include "../Common_App/ConnectionManager.h"
#include "../Common_App/RideSharingMessages.h"

#include "DriverMgmApp.h"

using namespace RideShare;
using namespace RideShare::Tools;
using namespace Decent;
using namespace Decent::Tools;
using namespace Decent::Ra::Message;

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
	std::cout << "================ Driver Management ================" << std::endl;

	TCLAP::CmdLine cmd("Driver Management", ' ', "ver", true);

	TCLAP::ValueArg<std::string> configPathArg("c", "config", "Path to the configuration file.", false, "Config.json", "String");
	TCLAP::ValueArg<std::string> wlKeyArg("w", "wl-key", "Key for the loaded whitelist.", false, "WhiteListKey", "String");
	TCLAP::SwitchArg isSendWlArg("s", "not-send-wl", "Do not send whitelist to Decent Server.", true);
	cmd.add(configPathArg);
	cmd.add(wlKeyArg);
	cmd.add(isSendWlArg);

	cmd.parse(argc, argv);

	std::string configJsonStr;
	if (!GetConfigurationJsonString(configPathArg.getValue(), configJsonStr))
	{
		PRINT_W("Failed to load configuration file.");
		return -1;
	}
	ConfigManager configManager(configJsonStr);
	ConnectionManager::SetConfigManager(configManager);

	const ConfigItem& decentServerItem = configManager.GetItem(Ra::WhiteList::sk_nameDecentServer);
	const ConfigItem& driMgmItem = configManager.GetItem(AppNames::sk_driverMgm);

	uint32_t serverIp = Net::TCPConnection::GetIpAddressFromStr(decentServerItem.GetAddr());
	std::unique_ptr<Net::Connection> serverCon;

	if (isSendWlArg.getValue())
	{
		serverCon = std::make_unique<Net::TCPConnection>(serverIp, decentServerItem.GetPort());
		serverCon->SendPack(LoadWhiteList(wlKeyArg.getValue(), configManager.GetLoadedWhiteListStr()));
	}

	serverCon = std::make_unique<Net::TCPConnection>(serverIp, decentServerItem.GetPort());

	std::shared_ptr<DriverMgm> enclave;
	try
	{
		enclave = std::make_shared<DriverMgm>(
			ENCLAVE_FILENAME, KnownFolderType::LocalAppDataEnclave, TOKEN_FILENAME, wlKeyArg.getValue(), *serverCon, 
			"DriverMgm Pay Info" + driMgmItem.GetAddr() + std::to_string(driMgmItem.GetPort()));
	}
	catch (const std::exception& e)
	{
		PRINT_W("Failed to start enclave program! Error Msg:\n%s", e.what());
		return -1;
	}


	Net::SmartServer smartServer;

	std::unique_ptr<Net::Server> server(std::make_unique<Net::TCPServer>(driMgmItem.GetAddr(), driMgmItem.GetPort()));

	smartServer.AddServer(server, enclave);
	smartServer.RunUtilUserTerminate();


	PRINT_I("Exit.");
	return 0;
}
