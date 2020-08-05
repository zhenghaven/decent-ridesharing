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

namespace
{
	static Ra::States& gs_state = Ra::GetStateSingleton();
}

bool RegesterCert(Net::ConnectionBase& con, const ComMsg::DriContact& contact);
std::unique_ptr<ComMsg::BestMatches> SendQuery(Net::ConnectionBase& con);
bool ConfirmMatch(Net::ConnectionBase& con, const ComMsg::DriContact& contact, const std::string& tripId);
bool TripStartOrEnd(Net::ConnectionBase& con, const std::string& tripId, const bool isStart);

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

	ComMsg::DriContact contact("driFirst driLast", "1234567890", "LicensePlateHere");

	std::unique_ptr<Net::ConnectionBase> appCon;

	Pause("register");
	appCon = ConnectionManager::GetConnection2DriverMgm(RequestCategory::sk_fromDriver);
	if (!RegesterCert(*appCon, contact))
	{
		return -1;
	}

	Pause("find closest passengers");
	std::unique_ptr<ComMsg::BestMatches> matches;
	appCon = ConnectionManager::GetConnection2TripMatcher(RequestCategory::sk_fromDriver);
	if (!(matches = SendQuery(*appCon)) ||
		matches->GetMatches().size() == 0)
	{
		return -1;
	}

	Pause("pick first closest passenger");
	appCon = ConnectionManager::GetConnection2TripMatcher(RequestCategory::sk_fromDriver);
	const ComMsg::MatchItem& firstItem = matches->GetMatches()[0];
	const std::string tripId = firstItem.GetTripId();
	if (!ConfirmMatch(*appCon, contact, tripId))
	{
		return -1;
	}

	Pause("start trip");
	appCon = ConnectionManager::GetConnection2TripMatcher(RequestCategory::sk_fromDriver);
	if (!TripStartOrEnd(*appCon, tripId, true))
	{
		return -1;
	}

	Pause("end trip");
	appCon = ConnectionManager::GetConnection2TripMatcher(RequestCategory::sk_fromDriver);
	if (!TripStartOrEnd(*appCon, tripId, false))
	{
		return -1;
	}

	Pause("exit");
	return 0;
}

bool RegesterCert(Net::ConnectionBase& con, const ComMsg::DriContact& contact)
{
	using namespace EncFunc::DriverMgm;
	using namespace MbedTlsObj;

	auto keyPair = EcKeyPair<EcKeyType::SECP256R1>(*(gs_state.GetKeyContainer().GetSignKeyPair()));

	X509ReqWriter certReq(HashType::SHA256, keyPair, "CN=Driver");

	Drbg drbg;
	ComMsg::DriReg regMsg(contact, "-Driver Pay Info-", certReq.GeneratePem(drbg), "DriLicense");

	std::shared_ptr<TlsConfigWithName> tlsCfg = std::make_shared<TlsConfigWithName>(gs_state, TlsConfigWithName::Mode::ClientNoCert, AppNames::sk_driverMgm, nullptr);
	Decent::Net::TlsCommLayer tls(con, tlsCfg, true, nullptr);

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

std::unique_ptr<ComMsg::BestMatches> SendQuery(Net::ConnectionBase& con)
{
	using namespace EncFunc::TripMatcher;

	ComMsg::DriverLoc DriLoc(ComMsg::Point2D<double>(1.1, 1.2));

	std::shared_ptr<TlsConfigWithName> tlsCfg = std::make_shared<TlsConfigWithName>(gs_state, TlsConfigWithName::Mode::ClientHasCert, AppNames::sk_tripMatcher, nullptr);
	TlsCommLayer tls(con, tlsCfg, true, nullptr);

	tls.SendStruct(k_findMatch);
	tls.SendContainer(DriLoc.ToString());
	std::string msgBuf = tls.RecvContainer<std::string>();

	std::unique_ptr<ComMsg::BestMatches> matchesMsg = ParseMsg<ComMsg::BestMatches>(msgBuf);

	const std::vector<ComMsg::MatchItem>& matches = matchesMsg->GetMatches();

	if (matches.size() == 0)
	{
		PRINT_I("No match found.");
	}
	else
	{
		PRINT_I("Found Matches:");
		for (const ComMsg::MatchItem& match : matches)
		{
			PRINT_I("\tID: %s, Ori: (%f, %f), Dest: (%f, %f).",
				match.GetTripId().c_str(),
				match.GetPath().GetPath().front().GetX(), match.GetPath().GetPath().front().GetY(),
				match.GetPath().GetPath().back().GetX(), match.GetPath().GetPath().back().GetY());
		}
	}

	return std::move(matchesMsg);
}

bool ConfirmMatch(Net::ConnectionBase& con, const ComMsg::DriContact& contact, const std::string& tripId)
{
	using namespace EncFunc::TripMatcher;

	ComMsg::DriSelection driSelection(contact, tripId);

	std::shared_ptr<TlsConfigWithName> tlsCfg = std::make_shared<TlsConfigWithName>(gs_state, TlsConfigWithName::Mode::ClientHasCert, AppNames::sk_tripMatcher, nullptr);
	Decent::Net::TlsCommLayer tls(con, tlsCfg, true, nullptr);

	tls.SendStruct(k_confirmMatch);
	tls.SendContainer(driSelection.ToString());
	std::string msgBuf = tls.RecvContainer<std::string>();
	std::unique_ptr<ComMsg::PasContact> pasContact = ParseMsg<ComMsg::PasContact>(msgBuf);

	PRINT_I("Matched Passenger:");
	PRINT_I("\tName:  %s.", pasContact->GetName().c_str());
	PRINT_I("\tPhone: %s.", pasContact->GetPhone().c_str());

	return true;
}

bool TripStartOrEnd(Net::ConnectionBase& con, const std::string& tripId, const bool isStart)
{
	using namespace EncFunc::TripMatcher;

	std::shared_ptr<TlsConfigWithName> tlsCfg = std::make_shared<TlsConfigWithName>(gs_state, TlsConfigWithName::Mode::ClientHasCert, AppNames::sk_tripMatcher, nullptr);
	Decent::Net::TlsCommLayer tls(con, tlsCfg, true, nullptr);

	tls.SendStruct(isStart ? k_tripStart : k_tripEnd);
	tls.SendContainer(tripId);

	PRINT_I("Trip %s.", isStart ? "started" : "ended");

	return true;
}
