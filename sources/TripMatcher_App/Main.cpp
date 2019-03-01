#include <string>
#include <memory>
#include <iostream>

#include <tclap/CmdLine.h>
#include <boost/asio/ip/address_v4.hpp>
#include <sgx_quote.h>

#include <DecentApi/CommonApp/Common.h>

#include <DecentApi/CommonApp/Net/SmartMessages.h>
#include <DecentApi/CommonApp/Net/TCPConnection.h>
#include <DecentApi/CommonApp/Net/TCPServer.h>
#include <DecentApi/CommonApp/Net/SmartServer.h>
#include <DecentApi/CommonApp/Ra/Messages.h>
#include <DecentApi/CommonApp/Tools/ConfigManager.h>

#include <DecentApi/Common/Ra/WhiteList/HardCoded.h>

#include "../Common/AppNames.h"
#include "../Common_App/Tools.h"
#include "../Common_App/ConnectionManager.h"
#include "../Common_App/RideSharingMessages.h"

#include "TripMatcherApp.h"

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
	std::cout << "================ Trip Matcher ================" << std::endl;

	TCLAP::CmdLine cmd("Trip Matcher", ' ', "ver", true);

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
		std::cout << "Failed to load configuration file." << std::endl;
		return -1;
	}
	ConfigManager configManager(configJsonStr);
	ConnectionManager::SetConfigManager(configManager);

	const ConfigItem& decentServerItem = configManager.GetItem(Ra::WhiteList::sk_nameDecentServer);
	const ConfigItem& tripMatcherItem = configManager.GetItem(AppNames::sk_tripMatcher);

	uint32_t serverIp = boost::asio::ip::address_v4::from_string(decentServerItem.GetAddr()).to_uint();
	std::unique_ptr<Net::Connection> serverCon;

	if (isSendWlArg.getValue())
	{
		serverCon = std::make_unique<Net::TCPConnection>(serverIp, decentServerItem.GetPort());
		serverCon->SendPack(LoadWhiteList(wlKeyArg.getValue(), configManager.GetLoadedWhiteListStr()));
	}

	serverCon = std::make_unique<Net::TCPConnection>(serverIp, decentServerItem.GetPort());

	std::shared_ptr<TripMatcher> enclave = std::make_shared<TripMatcher>(
		ENCLAVE_FILENAME, KnownFolderType::LocalAppDataEnclave, TOKEN_FILENAME, wlKeyArg.getValue(), *serverCon, 
		"TripMatcher Pay Info" + tripMatcherItem.GetAddr() + std::to_string(tripMatcherItem.GetPort()));


	Net::SmartServer smartServer;

	uint32_t tripMatcherIp = boost::asio::ip::address_v4::from_string(tripMatcherItem.GetAddr()).to_uint();
	std::unique_ptr<Net::Server> server(std::make_unique<Net::TCPServer>(tripMatcherIp, tripMatcherItem.GetPort()));

	smartServer.AddServer(server, enclave);
	smartServer.RunUtilUserTerminate();


	printf("Exit ...\n");
	return 0;
}
