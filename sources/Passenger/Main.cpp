#include <random>

#include <boost/filesystem.hpp>
#include <tclap/CmdLine.h>
#include <json/json.h>

#include <DecentApi/Common/Common.h>
#include <DecentApi/Common/Net/TlsCommLayer.h>
#include <DecentApi/Common/Tools/JsonTools.h>
#include <DecentApi/Common/Ra/Crypto.h>
#include <DecentApi/Common/Ra/ServerX509Cert.h>
#include <DecentApi/Common/Ra/ClientX509Cert.h>
#include <DecentApi/Common/Ra/TlsConfigWithName.h>
#include <DecentApi/Common/Ra/KeyContainer.h>
#include <DecentApi/Common/Ra/CertContainer.h>
#include <DecentApi/Common/Ra/WhiteList/LoadedList.h>
#include <DecentApi/Common/Ra/StatesSingleton.h>
#include <DecentApi/Common/MbedTls/EcKey.h>
#include <DecentApi/Common/MbedTls/Drbg.h>
#include <DecentApi/Common/MbedTls/X509Req.h>

#include <DecentApi/CommonApp/Net/TCPConnection.h>
#include <DecentApi/CommonApp/Tools/DiskFile.h>

#include <DecentApi/DecentAppApp/DecentAppConfig.h>

#include "../Common/AppNames.h"
#include "../Common/RideSharingMessages.h"
#include "../Common/RideSharingFuncNums.h"
#include "../Common_App/ConnectionManager.h"
#include "../Common_App/RequestCategory.h"

using namespace RideShare;
using namespace Decent;
using namespace Decent::Tools;
using namespace Decent::Net;
using namespace Decent::Ra;
using namespace Decent::Ra::WhiteList;
using namespace Decent::AppConfig;

bool RegesterCert(Net::ConnectionBase& con, const ComMsg::PasContact& contact);
bool SendQuery(Net::ConnectionBase& con, std::string& signedQuote);
bool ConfirmQuote(Net::ConnectionBase& con, const ComMsg::PasContact& contact, const std::string& signedQuoteStr, std::string& tripId);
bool TripStartOrEnd(Net::ConnectionBase& con, const std::string& tripId, const bool isStart);

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

	//------- Setup connection manager:
	ConnectionManager::SetEnclaveList(configMgr->GetEnclaveList());

	//------- Setup key and certificates:
	WhiteList::LoadedList loadedWhiteList(configMgr->GetEnclaveList().GetLoadedWhiteList().GetMap());

	//Setting Loaded whitelist.
	gs_state.GetLoadedWhiteList(&loadedWhiteList);

	//Setting up a temporary certificate.
	auto keyPair = MbedTlsObj::EcKeyPair<MbedTlsObj::EcKeyType::SECP256R1>(*(gs_state.GetKeyContainer().GetSignKeyPair()));
	Decent::Ra::ServerX509CertWriter certWrt(keyPair, "HashTemp", "PlatformTemp", "ReportTemp");

	MbedTlsObj::Drbg drbg;
	std::shared_ptr<Decent::Ra::ServerX509Cert> cert = std::make_shared<Decent::Ra::ServerX509Cert>(certWrt.GenerateDer(drbg));
	if (!cert)
	{
		return -1;
	}
	gs_state.GetCertContainer().SetCert(cert);

	ComMsg::PasContact contact("pasFirst pasLast", "1234567890");

	std::unique_ptr<Net::ConnectionBase> appCon;

	Pause("register");
	appCon = ConnectionManager::GetConnection2PassengerMgm(RequestCategory::sk_fromPassenger);
	if (!RegesterCert(*appCon, contact))
	{
		return -1;
	}

	Pause("get quote");
	std::string signedQuoteStr;
	appCon = ConnectionManager::GetConnection2TripPlanner(RequestCategory::sk_fromPassenger);
	if (!SendQuery(*appCon, signedQuoteStr))
	{
		return -1;
	}

	Pause("confirm quote");
	std::string tripId;
	appCon = ConnectionManager::GetConnection2TripMatcher(RequestCategory::sk_fromPassenger);
	if (!ConfirmQuote(*appCon, contact, signedQuoteStr, tripId))
	{
		return -1;
	}

	Pause("start trip");
	appCon = ConnectionManager::GetConnection2TripMatcher(RequestCategory::sk_fromPassenger);
	if (!TripStartOrEnd(*appCon, tripId, true))
	{
		return -1;
	}

	Pause("end trip");
	appCon = ConnectionManager::GetConnection2TripMatcher(RequestCategory::sk_fromPassenger);
	if (!TripStartOrEnd(*appCon, tripId, false))
	{
		return -1;
	}

	Pause("exit");
	return 0;
}

bool RegesterCert(Net::ConnectionBase& con, const ComMsg::PasContact& contact)
{
	using namespace EncFunc::PassengerMgm;
	using namespace MbedTlsObj;

	auto keyPair = EcKeyPair<EcKeyType::SECP256R1>(*(gs_state.GetKeyContainer().GetSignKeyPair()));

	Drbg drbg;
	X509ReqWriter certReqWrt(HashType::SHA256, keyPair, "CN=Passenger");

	ComMsg::PasReg regMsg(contact, "-Passneger Pay Info-", certReqWrt.GeneratePem(drbg));

	std::shared_ptr<TlsConfigWithName> tlsCfg = std::make_shared<TlsConfigWithName>(gs_state, TlsConfigWithName::Mode::ClientNoCert, AppNames::sk_passengerMgm, nullptr);
	TlsCommLayer tls(con, tlsCfg, true, nullptr);

	tls.SendStruct(k_userReg);
	tls.SendContainer(regMsg.ToString());
	std::string msgBuf = tls.RecvContainer<std::string>();

	std::shared_ptr<ClientX509Cert> cert = std::make_shared<ClientX509Cert>(msgBuf);
	if (!cert)
	{
		return false;
	}

	gs_state.GetCertContainer().SetCert(cert);


	return true;
}

bool SendQuery(Net::ConnectionBase& con, std::string& signedQuoteStr)
{
	using namespace EncFunc::TripPlaner;

	ComMsg::GetQuote getQuote(ComMsg::Point2D<double>(gs_locRandDis(gs_randGen), gs_locRandDis(gs_randGen)), 
		ComMsg::Point2D<double>(gs_locRandDis(gs_randGen), gs_locRandDis(gs_randGen)));

	std::shared_ptr<TlsConfigWithName> tlsCfg = std::make_shared<TlsConfigWithName>(gs_state, TlsConfigWithName::Mode::ClientHasCert, AppNames::sk_tripPlanner, nullptr);
	TlsCommLayer tls(con, tlsCfg, true, nullptr);

	tls.SendStruct(k_getQuote);
	tls.SendContainer(getQuote.ToString());
	signedQuoteStr = tls.RecvContainer<std::string>();

	std::unique_ptr<ComMsg::SignedQuote> signedQuote = ParseSignedQuote(signedQuoteStr, gs_state);
	std::unique_ptr<ComMsg::Quote> quote = ParseMsg<ComMsg::Quote>(signedQuote->GetQuote());

	PRINT_I("Received quote with price: %f.", quote->GetPrice().GetPrice());

	return true;
}

bool ConfirmQuote(Net::ConnectionBase& con, const ComMsg::PasContact& contact, const std::string& signedQuoteStr, std::string& tripId)
{
	using namespace EncFunc::TripMatcher;

	ComMsg::ConfirmQuote confirmQuote(contact, signedQuoteStr);

	std::shared_ptr<TlsConfigWithName> tlsCfg = std::make_shared<TlsConfigWithName>(gs_state, TlsConfigWithName::Mode::ClientHasCert, AppNames::sk_tripMatcher, nullptr);
	TlsCommLayer tls(con, tlsCfg, true, nullptr);

	tls.SendStruct(k_confirmQuote);
	tls.SendContainer(confirmQuote.ToString());
	PRINT_I("Waiting for a match...\n");
	std::string msgBuf = tls.RecvContainer<std::string>();

	std::unique_ptr<ComMsg::PasMatchedResult> matchedResult = ParseMsg<ComMsg::PasMatchedResult>(msgBuf);

	PRINT_I("Matched Driver:");
	PRINT_I("\tName:  %s.", matchedResult->GetDriContact().GetName().c_str());
	PRINT_I("\tPhone: %s.", matchedResult->GetDriContact().GetPhone().c_str());

	tripId = matchedResult->GetTripId();

	return true;
}

bool TripStartOrEnd(Net::ConnectionBase& con, const std::string& tripId, const bool isStart)
{
	using namespace EncFunc::TripMatcher;

	std::shared_ptr<TlsConfigWithName> tlsCfg = std::make_shared<TlsConfigWithName>(gs_state, TlsConfigWithName::Mode::ClientHasCert, AppNames::sk_tripMatcher, nullptr);
	TlsCommLayer tls(con, tlsCfg, true, nullptr);

	tls.SendStruct(isStart ? k_tripStart : k_tripEnd);
	tls.SendContainer(tripId);

	PRINT_I("Trip %s.", isStart ? "started" : "ended");

	return true;
}
