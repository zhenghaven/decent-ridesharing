#include <random>

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

bool RegesterCert(Net::Connection& con, const ComMsg::PasContact& contact);
bool SendQuery(Net::Connection& con, std::string& signedQuote);
bool ConfirmQuote(Net::Connection& con, const ComMsg::PasContact& contact, const std::string& signedQuoteStr, std::string& tripId);
bool TripStartOrEnd(Net::Connection& con, const std::string& tripId, const bool isStart);

namespace
{
	static std::random_device gs_randDev;
	static std::mt19937 gs_randGen(gs_randDev());
	static std::uniform_real_distribution<> gs_locRandDis(-5.0, 5.0);

	static Ra::States& gs_state = Ra::GetStateSingleton();
}

template<typename MsgType>
static std::unique_ptr<MsgType> ParseMsg(const std::string& msgStr)
{
	JsonDoc json;
	Decent::Tools::ParseStr2Json(json, msgStr);
	return std::make_unique<MsgType>(json);
}

static std::unique_ptr<ComMsg::SignedQuote> ParseSignedQuote(const std::string& msg, Ra::States& state)
{
	JsonDoc json;
	ParseStr2Json(json, msg);

	return std::make_unique<ComMsg::SignedQuote>(ComMsg::SignedQuote::ParseSignedQuote(json, state, AppNames::sk_tripPlanner));
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
	std::cout << "================ Passenger ================" << std::endl;

	TCLAP::CmdLine cmd("Passenger", ' ', "ver", true);

	TCLAP::ValueArg<std::string> configPathArg("c", "config", "Path to the configuration file.", false, "Config.json", "String");
	cmd.add(configPathArg);

	cmd.parse(argc, argv);

	std::string configJsonStr;
	if (!GetConfigurationJsonString(configPathArg.getValue(), configJsonStr))
	{
		std::cout << "Failed to load configuration file." << std::endl;
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

	ComMsg::PasContact contact("pasFirst pasLast", "1234567890");

	std::unique_ptr<Net::Connection> appCon;

	Pause("register");
	appCon = ConnectionManager::GetConnection2PassengerMgm(FromPassenger());
	if (!RegesterCert(*appCon, contact))
	{
		return -1;
	}

	Pause("get quote");
	std::string signedQuoteStr;
	appCon = ConnectionManager::GetConnection2TripPlanner(FromPassenger());
	if (!SendQuery(*appCon, signedQuoteStr))
	{
		return -1;
	}

	Pause("confirm quote");
	std::string tripId;
	appCon = ConnectionManager::GetConnection2TripMatcher(FromPassenger());
	if (!ConfirmQuote(*appCon, contact, signedQuoteStr, tripId))
	{
		return -1;
	}

	Pause("start trip");
	appCon = ConnectionManager::GetConnection2TripMatcher(FromPassenger());
	if (!TripStartOrEnd(*appCon, tripId, true))
	{
		return -1;
	}

	Pause("end trip");
	appCon = ConnectionManager::GetConnection2TripMatcher(FromPassenger());
	if (!TripStartOrEnd(*appCon, tripId, false))
	{
		return -1;
	}

	Pause("exit");
	return 0;
}

bool RegesterCert(Net::Connection& con, const ComMsg::PasContact& contact)
{
	using namespace EncFunc::PassengerMgm;

	Ra::X509Req certReq(*gs_state.GetKeyContainer().GetSignKeyPair(), "Passenger");

	ComMsg::PasReg regMsg(contact, "-Passneger Pay Info-", certReq.ToPemString());

	std::shared_ptr<TlsConfigWithName> tlsCfg = std::make_shared<TlsConfigWithName>(gs_state, TlsConfig::Mode::ClientNoCert, AppNames::sk_passengerMgm);
	TlsCommLayer tls(&con, tlsCfg, true);

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

bool SendQuery(Net::Connection& con, std::string& signedQuoteStr)
{
	using namespace EncFunc::TripPlaner;

	ComMsg::GetQuote getQuote(ComMsg::Point2D<double>(gs_locRandDis(gs_randGen), gs_locRandDis(gs_randGen)), 
		ComMsg::Point2D<double>(gs_locRandDis(gs_randGen), gs_locRandDis(gs_randGen)));

	std::shared_ptr<TlsConfigWithName> tlsCfg = std::make_shared<TlsConfigWithName>(gs_state, TlsConfig::Mode::ClientHasCert, AppNames::sk_tripPlanner);
	TlsCommLayer tls(&con, tlsCfg, true);

	tls.SendStruct(k_getQuote);
	tls.SendMsg(getQuote.ToString());
	tls.ReceiveMsg(signedQuoteStr);

	std::unique_ptr<ComMsg::SignedQuote> signedQuote = ParseSignedQuote(signedQuoteStr, gs_state);
	std::unique_ptr<ComMsg::Quote> quote = ParseMsg<ComMsg::Quote>(signedQuote->GetQuote());

	PRINT_I("Received quote with price: %f.", quote->GetPrice().GetPrice());

	return true;
}

bool ConfirmQuote(Net::Connection& con, const ComMsg::PasContact& contact, const std::string& signedQuoteStr, std::string& tripId)
{
	using namespace EncFunc::TripMatcher;

	ComMsg::ConfirmQuote confirmQuote(contact, signedQuoteStr);

	std::shared_ptr<TlsConfigWithName> tlsCfg = std::make_shared<TlsConfigWithName>(gs_state, TlsConfig::Mode::ClientHasCert, AppNames::sk_tripMatcher);
	TlsCommLayer tls(&con, tlsCfg, true);
	
	std::string msgBuf;

	tls.SendStruct(k_confirmQuote);
	tls.SendMsg(confirmQuote.ToString());
	tls.ReceiveMsg(msgBuf);

	std::unique_ptr<ComMsg::PasMatchedResult> matchedResult = ParseMsg<ComMsg::PasMatchedResult>(msgBuf);

	PRINT_I("Matched Driver:");
	PRINT_I("\tName:  %s.", matchedResult->GetDriContact().GetName().c_str());
	PRINT_I("\tPhone: %s.", matchedResult->GetDriContact().GetPhone().c_str());

	tripId = matchedResult->GetTripId();

	return true;
}

bool TripStartOrEnd(Net::Connection& con, const std::string& tripId, const bool isStart)
{
	using namespace EncFunc::TripMatcher;

	std::shared_ptr<TlsConfigWithName> tlsCfg = std::make_shared<TlsConfigWithName>(gs_state, TlsConfig::Mode::ClientHasCert, AppNames::sk_tripMatcher);
	TlsCommLayer tls(&con, tlsCfg, true);

	tls.SendStruct(isStart ? k_tripStart : k_tripEnd);
	tls.SendMsg(tripId);

	PRINT_I("Trip %s.", isStart ? "started" : "ended");

	return true;
}
