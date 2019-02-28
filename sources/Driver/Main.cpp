
#include <tclap/CmdLine.h>
#include <boost/asio/ip/address_v4.hpp>
#include <json/json.h>

#include <DecentApi/Common/Common.h>
#include <DecentApi/Common/Net/TlsCommLayer.h>
#include <DecentApi/Common/Tools/JsonTools.h>
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

bool RegesterCert(Net::Connection& con, const ComMsg::DriContact& contact);
std::unique_ptr<ComMsg::BestMatches> SendQuery(Net::Connection& con);
bool ConfirmMatch(Net::Connection& con, const ComMsg::DriContact& contact, const std::string& tripId);
bool TripStartOrEnd(Net::Connection& con, const std::string& tripId, const bool isStart);

template<typename MsgType>
static std::unique_ptr<MsgType> ParseMsg(const std::string& msgStr)
{
	try
	{
		JsonDoc json;
		return ParseStr2Json(json, msgStr) ?
			std::make_unique<MsgType>(json) :
			nullptr;
	}
	catch (const std::exception&)
	{
		return nullptr;
	}
}

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
	std::cout << "================ Driver ================" << std::endl;

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

	ComMsg::DriContact contact("driFirst driLast", "1234567890", "LicensePlateHere");

	std::unique_ptr<Net::Connection> appCon;

	appCon = ConnectionManager::GetConnection2DriverMgm(FromDriver());
	if (!RegesterCert(*appCon, contact))
	{
		return 1;
	}

	std::unique_ptr<ComMsg::BestMatches> matches;
	appCon = ConnectionManager::GetConnection2TripMatcher(FromDriver());
	if (!(matches = SendQuery(*appCon)) ||
		matches->GetMatches().size() == 0)
	{
		return 1;
	}

	appCon = ConnectionManager::GetConnection2TripMatcher(FromDriver());
	const ComMsg::MatchItem& firstItem = matches->GetMatches()[0];
	const std::string tripId = firstItem.GetTripId();
	if (!ConfirmMatch(*appCon, contact, tripId))
	{
		return 1;
	}

	appCon = ConnectionManager::GetConnection2TripMatcher(FromDriver());
	if (!TripStartOrEnd(*appCon, tripId, true))
	{
		return 1;
	}

	appCon = ConnectionManager::GetConnection2TripMatcher(FromDriver());
	if (!TripStartOrEnd(*appCon, tripId, false))
	{
		return 1;
	}

	LOGI("Exit.\n");
	return 0;
}

bool RegesterCert(Net::Connection& con, const ComMsg::DriContact& contact)
{
	using namespace EncFunc::DriverMgm;
	Ra::States& state = Ra::States::Get();

	Ra::X509Req certReq(*state.GetKeyContainer().GetSignKeyPair(), "Driver");

	ComMsg::DriReg regMsg(contact, "Driver Payment Info XXXXX", certReq.ToPemString(), "DriLicense");

	std::shared_ptr<Decent::Ra::TlsConfig> pasTlsCfg = std::make_shared<Decent::Ra::TlsConfig>(AppNames::sk_passengerMgm, state);
	Decent::Net::TlsCommLayer pasTls(&con, pasTlsCfg, true);

	std::string msgBuf;

	if (!pasTls.SendMsg(&con, EncFunc::GetMsgString(k_userReg)) ||
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

std::unique_ptr<ComMsg::BestMatches> SendQuery(Net::Connection& con)
{
	using namespace EncFunc::TripMatcher;
	Ra::States& state = Ra::States::Get();

	ComMsg::DriverLoc DriLoc(ComMsg::Point2D<double>(1.1, 1.2));

	std::shared_ptr<Decent::Ra::TlsConfig> tmTlsCfg = std::make_shared<Decent::Ra::TlsConfig>(AppNames::sk_tripMatcher, state, false);
	Decent::Net::TlsCommLayer tmTls(&con, tmTlsCfg, true);

	std::string msgBuf;
	if (!tmTls.SendMsg(&con, EncFunc::GetMsgString(k_findMatch)) ||
		!tmTls.SendMsg(&con, DriLoc.ToString()) ||
		!tmTls.ReceiveMsg(&con, msgBuf) )
	{
		return false;
	}

	return ParseMsg<ComMsg::BestMatches>(msgBuf);
}

bool ConfirmMatch(Net::Connection& con, const ComMsg::DriContact& contact, const std::string& tripId)
{
	using namespace EncFunc::TripMatcher;
	Ra::States& state = Ra::States::Get();

	ComMsg::DriSelection driSelection(contact, tripId);

	std::shared_ptr<Decent::Ra::TlsConfig> tmTlsCfg = std::make_shared<Decent::Ra::TlsConfig>(AppNames::sk_tripMatcher, state, false);
	Decent::Net::TlsCommLayer tmTls(&con, tmTlsCfg, true);
	
	std::string msgBuf;
	std::unique_ptr<ComMsg::PasContact> pasContact;
	if (!tmTls.SendMsg(&con, EncFunc::GetMsgString(k_confirmMatch)) ||
		!tmTls.SendMsg(&con, driSelection.ToString()) ||
		!tmTls.ReceiveMsg(&con, msgBuf) ||
		!(pasContact = ParseMsg<ComMsg::PasContact>(msgBuf)) )
	{
		return false;
	}

	LOGI("Matched Passenger:");
	LOGI("\tName:  %s.", pasContact->GetName().c_str());
	LOGI("\tPhone: %s.", pasContact->GetPhone().c_str());

	return true;
}

bool TripStartOrEnd(Net::Connection& con, const std::string& tripId, const bool isStart)
{
	using namespace EncFunc::TripMatcher;
	Ra::States& state = Ra::States::Get();

	std::shared_ptr<Decent::Ra::TlsConfig> tmTlsCfg = std::make_shared<Decent::Ra::TlsConfig>(AppNames::sk_tripMatcher, state, false);
	Decent::Net::TlsCommLayer tmTls(&con, tmTlsCfg, true);

	if (!tmTls.SendMsg(&con, EncFunc::GetMsgString(isStart ? k_tripStart : k_tripEnd)) ||
		!tmTls.SendMsg(&con, tripId))
	{
		LOGW("Failed to send trip %s.", isStart ? "start" : "end");
		return false;
	}

	LOGI("Trip %s.", isStart ? "started" : "ended");

	return true;
}
