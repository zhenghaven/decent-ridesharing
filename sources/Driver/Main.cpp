
#include <tclap/CmdLine.h>
#include <json/json.h>

#include <DecentApi/Common/Common.h>
#include <DecentApi/Common/Net/TlsCommLayer.h>
#include <DecentApi/Common/Tools/JsonTools.h>
#include <DecentApi/Common/Ra/Crypto.h>
#include <DecentApi/Common/Ra/ClientX509.h>
#include <DecentApi/Common/Ra/TlsConfigWithName.h>
#include <DecentApi/Common/Ra/KeyContainer.h>
#include <DecentApi/Common/Ra/CertContainer.h>
#include <DecentApi/Common/Ra/WhiteList/Loaded.h>
#include <DecentApi/Common/Ra/StatesSingleton.h>

#include <DecentApi/CommonApp/Net/TCPConnection.h>
#include <DecentApi/CommonApp/Tools/ConfigManager.h>

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
using namespace Decent::Net;
using namespace Decent::Ra;
using namespace Decent::Ra::WhiteList;

namespace
{
	static Ra::States& gs_state = Ra::GetStateSingleton();
}

bool RegesterCert(Net::Connection& con, const ComMsg::DriContact& contact);
std::unique_ptr<ComMsg::BestMatches> SendQuery(Net::Connection& con);
bool ConfirmMatch(Net::Connection& con, const ComMsg::DriContact& contact, const std::string& tripId);
bool TripStartOrEnd(Net::Connection& con, const std::string& tripId, const bool isStart);

template<typename MsgType>
static std::unique_ptr<MsgType> ParseMsg(const std::string& msgStr)
{
	JsonDoc json;
	Decent::Tools::ParseStr2Json(json, msgStr);
	return std::make_unique<MsgType>(json);
}

static void Pause(const std::string& msg)
{
	std::cout << "Press enter to " << msg << "...";
	std::string buf;
	std::getline(std::cin, buf);
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
		PRINT_I("Failed to load configuration file.");
		return -1;
	}
	ConfigManager configManager(configJsonStr);
	ConnectionManager::SetConfigManager(configManager);

	Loaded loadedWhiteList(configManager.GetLoadedWhiteList().GetMap());

	//Setting Loaded whitelist.
	gs_state.GetLoadedWhiteList(&loadedWhiteList);

	//Setting up a temporary certificate.
	std::shared_ptr<Decent::Ra::ServerX509> cert = std::make_shared<Decent::Ra::ServerX509>(
		*gs_state.GetKeyContainer().GetSignKeyPair(), "HashTemp", "PlatformTemp", "ReportTemp"
		);
	if (!cert || !*cert)
	{
		return -1;
	}
	gs_state.GetCertContainer().SetCert(cert);

	ComMsg::DriContact contact("driFirst driLast", "1234567890", "LicensePlateHere");

	std::unique_ptr<Net::Connection> appCon;

	Pause("register");
	appCon = ConnectionManager::GetConnection2DriverMgm(FromDriver());
	if (!RegesterCert(*appCon, contact))
	{
		return -1;
	}

	Pause("find closest passengers");
	std::unique_ptr<ComMsg::BestMatches> matches;
	appCon = ConnectionManager::GetConnection2TripMatcher(FromDriver());
	if (!(matches = SendQuery(*appCon)) ||
		matches->GetMatches().size() == 0)
	{
		return -1;
	}

	Pause("pick first closest passenger");
	appCon = ConnectionManager::GetConnection2TripMatcher(FromDriver());
	const ComMsg::MatchItem& firstItem = matches->GetMatches()[0];
	const std::string tripId = firstItem.GetTripId();
	if (!ConfirmMatch(*appCon, contact, tripId))
	{
		return -1;
	}

	Pause("start trip");
	appCon = ConnectionManager::GetConnection2TripMatcher(FromDriver());
	if (!TripStartOrEnd(*appCon, tripId, true))
	{
		return -1;
	}

	Pause("end trip");
	appCon = ConnectionManager::GetConnection2TripMatcher(FromDriver());
	if (!TripStartOrEnd(*appCon, tripId, false))
	{
		return -1;
	}

	Pause("exit");
	return 0;
}

bool RegesterCert(Net::Connection& con, const ComMsg::DriContact& contact)
{
	using namespace EncFunc::DriverMgm;

	Ra::X509Req certReq(*gs_state.GetKeyContainer().GetSignKeyPair(), "Driver");

	ComMsg::DriReg regMsg(contact, "-Driver Pay Info-", certReq.ToPemString(), "DriLicense");

	std::shared_ptr<TlsConfigWithName> tlsCfg = std::make_shared<TlsConfigWithName>(gs_state, TlsConfig::Mode::ClientNoCert, AppNames::sk_driverMgm);
	Decent::Net::TlsCommLayer tls(&con, tlsCfg, true);

	std::string msgBuf;

	tls.SendStruct(k_userReg);
	tls.SendMsg(regMsg.ToString());
	tls.ReceiveMsg(msgBuf);

	std::shared_ptr<ClientX509> cert = std::make_shared<ClientX509>(msgBuf);
	if (!cert || !*cert)
	{
		return false;
	}

	gs_state.GetCertContainer().SetCert(cert);


	return true;
}

std::unique_ptr<ComMsg::BestMatches> SendQuery(Net::Connection& con)
{
	using namespace EncFunc::TripMatcher;

	ComMsg::DriverLoc DriLoc(ComMsg::Point2D<double>(1.1, 1.2));

	std::shared_ptr<TlsConfigWithName> tlsCfg = std::make_shared<TlsConfigWithName>(gs_state, TlsConfig::Mode::ClientHasCert, AppNames::sk_tripMatcher);
	TlsCommLayer tls(&con, tlsCfg, true);

	std::string msgBuf;
	tls.SendStruct(k_findMatch);
	tls.SendMsg(DriLoc.ToString());
	tls.ReceiveMsg(msgBuf);

	std::unique_ptr<ComMsg::BestMatches> matchesMsg = ParseMsg<ComMsg::BestMatches>(msgBuf);

	const std::vector<ComMsg::MatchItem>& matches = matchesMsg->GetMatches();
	PRINT_I("Found Matches:");
	for (const ComMsg::MatchItem& match : matches)
	{
		PRINT_I("\tID: %s, Ori: (%f, %f), Dest: (%f, %f).",
			match.GetTripId().c_str(),
			match.GetPath().GetPath().front().GetX(), match.GetPath().GetPath().front().GetY(),
			match.GetPath().GetPath().back().GetX(), match.GetPath().GetPath().back().GetY());
	}

	return std::move(matchesMsg);
}

bool ConfirmMatch(Net::Connection& con, const ComMsg::DriContact& contact, const std::string& tripId)
{
	using namespace EncFunc::TripMatcher;

	ComMsg::DriSelection driSelection(contact, tripId);

	std::shared_ptr<TlsConfigWithName> tlsCfg = std::make_shared<TlsConfigWithName>(gs_state, TlsConfig::Mode::ClientHasCert, AppNames::sk_tripMatcher);
	Decent::Net::TlsCommLayer tls(&con, tlsCfg, true);
	
	std::string msgBuf;

	tls.SendStruct(k_confirmMatch);
	tls.SendMsg(driSelection.ToString());
	tls.ReceiveMsg(msgBuf);
	std::unique_ptr<ComMsg::PasContact> pasContact = ParseMsg<ComMsg::PasContact>(msgBuf);

	PRINT_I("Matched Passenger:");
	PRINT_I("\tName:  %s.", pasContact->GetName().c_str());
	PRINT_I("\tPhone: %s.", pasContact->GetPhone().c_str());

	return true;
}

bool TripStartOrEnd(Net::Connection& con, const std::string& tripId, const bool isStart)
{
	using namespace EncFunc::TripMatcher;

	std::shared_ptr<TlsConfigWithName> tlsCfg = std::make_shared<TlsConfigWithName>(gs_state, TlsConfig::Mode::ClientHasCert, AppNames::sk_tripMatcher);
	Decent::Net::TlsCommLayer tls(&con, tlsCfg, true);

	tls.SendStruct(isStart ? k_tripStart : k_tripEnd);
	tls.SendMsg(tripId);

	PRINT_I("Trip %s.", isStart ? "started" : "ended");

	return true;
}
