
#include <tclap/CmdLine.h>
#include <boost/asio/ip/address_v4.hpp>

#include <DecentApi/Common/Common.h>
#include <DecentApi/Common/Net/TlsCommLayer.h>
#include <DecentApi/Common/Ra/States.h>
#include <DecentApi/Common/Ra/Crypto.h>
#include <DecentApi/Common/Ra/TlsConfig.h>
#include <DecentApi/Common/Ra/KeyContainer.h>
#include <DecentApi/Common/Ra/CertContainer.h>
#include <DecentApi/CommonApp/Net/TCPConnection.h>

#include "../Common/Crypto.h"
#include "../Common/TlsConfig.h"
#include "../Common/RideSharingMessages.h"
#include "../Common/RideSharingFuncNums.h"
#include "../Common_App/RideSharingMessages.h"

using namespace Decent;
using namespace RideShare;

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

#ifndef DEBUG
	TCLAP::ValueArg<uint16_t>  argServerPort("p", "port", "Port number for on-coming local connection.", true, 0, "[0-65535]");
	cmd.add(argServerPort);
#else
	TCLAP::ValueArg<int> testOpt("t", "test-opt", "Test Option Number", false, 0, "A single digit number.");
	cmd.add(testOpt);
#endif

	cmd.parse(argc, argv);

#ifndef DEBUG
	std::string appAddr = "127.0.0.1";
	uint16_t appPort = argServerPort.getValue();
#else
	uint16_t rootAppPort = 57755U;

	std::string appAddr = "127.0.0.1";
	uint16_t pasMgmPort = rootAppPort + testOpt.getValue() + 1;
	uint16_t tpPort = rootAppPort + testOpt.getValue() + 2;
#endif

	uint32_t appIp = boost::asio::ip::address_v4::from_string(appAddr).to_uint();

	Ra::States& state = Ra::States::Get();
	std::shared_ptr<Decent::Ra::ServerX509> cert = std::make_shared<Decent::Ra::ServerX509>(
		*state.GetKeyContainer().GetSignKeyPair(), "HashTemp", "PlatformTemp", "ReportTemp"
		);
	if (!cert || !*cert)
	{
		return 1;
	}
	state.GetCertContainer().SetCert(cert);

	std::unique_ptr<Net::Connection> appCon = std::make_unique<Net::TCPConnection>(appIp, pasMgmPort);
	if (!RegesterCert(*appCon))
	{
		return 1;
	}

	appCon = std::make_unique<Net::TCPConnection>(appIp, tpPort);
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

	con.SendPack(FromPassenger());

	std::shared_ptr<Decent::Ra::TlsConfig> pasTlsCfg = std::make_shared<Decent::Ra::TlsConfig>(""/*TODO*/, state);
	Decent::Net::TlsCommLayer pasTls(&con, pasTlsCfg, true);

	std::string msgBuf;
	msgBuf.resize(sizeof(EncFunc::PassengerMgm::NumType));
	EncFunc::PassengerMgm::NumType& funcNum = *reinterpret_cast<EncFunc::PassengerMgm::NumType*>(&msgBuf[0]);
	funcNum = EncFunc::PassengerMgm::k_userReg;

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

	con.SendPack(FromPassenger());

	std::shared_ptr<Decent::Ra::TlsConfig> pasTlsCfg = std::make_shared<Decent::Ra::TlsConfig>(""/*TODO*/, state, false);
	Decent::Net::TlsCommLayer pasTls(&con, pasTlsCfg, true);

	std::string msgBuf;
	msgBuf.resize(sizeof(EncFunc::TripPlaner::NumType));
	EncFunc::TripPlaner::NumType& funcNum = *reinterpret_cast<EncFunc::TripPlaner::NumType*>(&msgBuf[0]);
	funcNum = EncFunc::TripPlaner::k_getQuote;

	if (!pasTls.SendMsg(&con, msgBuf) ||
		!pasTls.SendMsg(&con, getQuote.ToString()))
	{
		return false;
	}

	return true;
}
