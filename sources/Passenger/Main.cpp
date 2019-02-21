
#include <tclap/CmdLine.h>
#include <boost/asio/ip/address_v4.hpp>

#include <DecentApi/Common/Common.h>
#include <DecentApi/Common/Net/TlsCommLayer.h>
#include <DecentApi/Common/Ra/States.h>
#include <DecentApi/Common/Ra/Crypto.h>
#include <DecentApi/Common/Ra/TlsConfig.h>
#include <DecentApi/Common/Ra/KeyContainer.h>
#include <DecentApi/Common/Ra/CertContainer.h>
#include <DecentApi/Common/Ra/WhiteList/Loaded.h>

#include <DecentApi/CommonApp/Net/TCPConnection.h>
#include <DecentApi/CommonApp/Tools/ConfigManager.h>

#include "../Common/Crypto.h"
#include "../Common/TlsConfig.h"
#include "../Common/AppNames.h"
#include "../Common/RideSharingMessages.h"
#include "../Common/RideSharingFuncNums.h"
#include "../Common_App/Tools.h"
#include "../Common_App/ConnectionManager.h"
#include "../Common_App/RideSharingMessages.h"

using namespace RideShare;
using namespace RideShare::Tools;
using namespace Decent;
using namespace Decent::Tools;
using namespace Decent::Ra::WhiteList;

bool RegesterCert(Net::Connection& con);
bool SendQuery(Net::Connection& con);

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
	std::cout << "================ Passenger ================" << std::endl;

	TCLAP::CmdLine cmd("Passenger", ' ', "ver", true);

	TCLAP::ValueArg<std::string> configPathArg("c", "config", "Path to the configuration file.", false, "Config.json", "String");
	cmd.add(configPathArg);

	cmd.parse(argc, argv);

	std::string configJsonStr;
	if (!GetConfigurationJsonString(configPathArg.getValue(), configJsonStr))
	{
		std::cout << "Failed to load configuration file." << std::endl;
		return 1;
	}
	ConfigManager configManager(configJsonStr);
	ConnectionManager::SetConfigManager(configManager);

	Loaded loadedWhiteList(configManager.GetLoadedWhiteList().GetMap());

	//Setting Loaded whitelist.
	Ra::States& state = Ra::States::Get();
	state.GetLoadedWhiteList(&loadedWhiteList);

	//Setting up a temporary certificate.
	std::shared_ptr<Decent::Ra::ServerX509> cert = std::make_shared<Decent::Ra::ServerX509>(
		*state.GetKeyContainer().GetSignKeyPair(), "HashTemp", "PlatformTemp", "ReportTemp"
		);
	if (!cert || !*cert)
	{
		return 1;
	}
	state.GetCertContainer().SetCert(cert);

	std::unique_ptr<Net::Connection> appCon;

	appCon = ConnectionManager::GetConnection2PassengerMgm(FromPassenger());
	if (!RegesterCert(*appCon))
	{
		return 1;
	}

	appCon = ConnectionManager::GetConnection2TripPlanner(FromPassenger());
	if (!SendQuery(*appCon))
	{
		return 1;
	}

	LOGI("Exit.\n");
	return 0;
}

bool RegesterCert(Net::Connection& con)
{
	Ra::States& state = Ra::States::Get();

	Ra::X509Req certReq(*state.GetKeyContainer().GetSignKeyPair(), "Passenger");

	ComMsg::PasReg regMsg(ComMsg::PasContact("fName lName", "1234567890"), "Passneger Payment Info XXXXX", certReq.ToPemString());

	std::shared_ptr<Decent::Ra::TlsConfig> pasTlsCfg = std::make_shared<Decent::Ra::TlsConfig>(AppNames::sk_passengerMgm, state);
	Decent::Net::TlsCommLayer pasTls(&con, pasTlsCfg, true);

	std::string msgBuf = EncFunc::GetMsgString(EncFunc::PassengerMgm::k_userReg);

	if (!pasTls.SendMsg(&con, msgBuf) ||
		!pasTls.SendMsg(&con, regMsg.ToString()) ||
		!pasTls.ReceiveMsg(&con, msgBuf))
	{
		return false;
	}

	std::shared_ptr<ClientX509> cert = std::make_shared<ClientX509>(msgBuf);
	if (!cert || !*cert)
	{
		return false;
	}

	state.GetCertContainer().SetCert(cert);


	return true;
}

bool SendQuery(Net::Connection& con)
{
	Ra::States& state = Ra::States::Get();

	ComMsg::GetQuote getQuote(ComMsg::Point2D<double>(1.1, 1.2), ComMsg::Point2D<double>(1.3, 1.4));

	std::shared_ptr<Decent::Ra::TlsConfig> tpTlsCfg = std::make_shared<Decent::Ra::TlsConfig>(AppNames::sk_tripPlanner, state, false);
	Decent::Net::TlsCommLayer tpTls(&con, tpTlsCfg, true);

	std::string msgBuf = EncFunc::GetMsgString(EncFunc::TripPlaner::k_getQuote);

	if (!tpTls.SendMsg(&con, msgBuf) ||
		!tpTls.SendMsg(&con, getQuote.ToString()))
	{
		return false;
	}

	return true;
}
