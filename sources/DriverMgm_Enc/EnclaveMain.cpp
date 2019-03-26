#include <mutex>
#include <memory>
#include <map>

#include <DecentApi/Common/Common.h>
#include <DecentApi/Common/make_unique.h>
#include <DecentApi/Common/Ra/ClientX509.h>
#include <DecentApi/Common/Ra/TlsConfigWithName.h>
#include <DecentApi/Common/Ra/KeyContainer.h>
#include <DecentApi/Common/Ra/CertContainer.h>
#include <DecentApi/Common/Net/TlsCommLayer.h>
#include <DecentApi/Common/Tools/JsonTools.h>
#include <DecentApi/DecentAppEnclave/AppStatesSingleton.h>

#include <rapidjson/document.h>
#include <cppcodec/base64_default_rfc4648.hpp>

#include "../Common/AppNames.h"
#include "../Common/RideSharingFuncNums.h"
#include "../Common/RideSharingMessages.h"

#include "../Common_Enc/OperatorPayment.h"

using namespace RideShare;
using namespace Decent::Ra;

namespace
{
	static AppStates& gs_state = GetAppStateSingleton();
	
	struct DriProfileItem
	{
		ComMsg::DriContact m_contact;
		std::string m_pay;
		std::string m_driLic;

		DriProfileItem(const ComMsg::DriContact& contact, const std::string& pay, const std::string& driLic) :
			m_contact(contact),
			m_pay(pay),
			m_driLic(driLic)
		{}
		DriProfileItem(ComMsg::DriContact&& contact, std::string&& pay, std::string&& driLic) :
			m_contact(std::forward<ComMsg::DriContact>(contact)),
			m_pay(std::forward<std::string>(pay)),
			m_driLic(std::forward<std::string>(driLic))
		{}

		DriProfileItem(DriProfileItem&& rhs) :
			DriProfileItem(std::forward<ComMsg::DriContact>(rhs.m_contact), 
				std::forward<std::string>(rhs.m_pay), std::forward<std::string>(rhs.m_driLic))
		{}
	};

	typedef std::map<std::string, DriProfileItem> ProfileMapType;
	ProfileMapType gs_profileMap;
	const ProfileMapType& gsk_profileMap = gs_profileMap;
	std::mutex gs_profileMapMutex;

	template<typename MsgType>
	static std::unique_ptr<MsgType> ParseMsg(const std::string& msgStr)
	{
		rapidjson::Document json;
		Decent::Tools::ParseStr2Json(json, msgStr);
		return Decent::Tools::make_unique<MsgType>(json);
	}
}

static bool VerifyContactInfo(const ComMsg::DriContact& contact)
{
	return contact.GetName().size() != 0 && contact.GetPhone().size() != 0;
}

static bool VerifyDriverLicense(const std::string& driLic)
{
	return driLic.size() != 0;
}

static bool VerifyPayment(const std::string& pay)
{
	return pay.size() != 0;
}

static void ProcessDriRegisterReq(void* const connection, Decent::Net::TlsCommLayer& tls)
{
	LOGI("Processing Driver Register Request...");

	std::string msgBuf;


	tls.ReceiveMsg(connection, msgBuf);
	std::unique_ptr<ComMsg::DriReg> driRegInfo = ParseMsg<ComMsg::DriReg>(msgBuf);

	if (!VerifyContactInfo(driRegInfo->GetContact()) ||
		!VerifyDriverLicense(driRegInfo->GetDriLic()) ||
		!VerifyPayment(driRegInfo->GetPayment()) )
	{
		return;
	}

	const std::string& csrPem = driRegInfo->GetCsr();
	X509Req certReq(csrPem);

	if (!certReq)
	{
		return;
	}

	const std::string pubPemStr = certReq.GetEcPublicKey().ToPubPemString();

	{
		std::unique_lock<std::mutex> profileMapLock(gs_profileMapMutex);
		if (gsk_profileMap.find(pubPemStr) != gsk_profileMap.cend())
		{
			LOGI("Client profile already exist.");
			return;
		}
	}

	std::shared_ptr<const Decent::MbedTlsObj::ECKeyPair> prvKey = gs_state.GetKeyContainer().GetSignKeyPair();
	std::shared_ptr<const AppX509> cert = gs_state.GetAppCertContainer().GetAppCert();

	if (!certReq.VerifySignature() ||
		!certReq.GetEcPublicKey() ||
		!prvKey || !*prvKey ||
		!cert || !*cert)
	{
		return;
	}

	const ComMsg::DriContact& contact = driRegInfo->GetContact();
	ClientX509 clientCert(certReq.GetEcPublicKey(), *cert, *prvKey, "Decent_RideShare_Driver_",
		cppcodec::base64_rfc4648::encode(contact.CalcHash()));

	{
		std::unique_lock<std::mutex> pasProfilesLock(gs_profileMapMutex);

		gs_profileMap.insert(std::make_pair(pubPemStr,
			DriProfileItem(contact, driRegInfo->GetPayment(), driRegInfo->GetDriLic())));

		LOGI("Client profile added. Profile store size: %llu.", gsk_profileMap.size());
	}

	tls.SendMsg(connection, clientCert.ToPemString());

}

static void LogQuery(void* const connection, Decent::Net::TlsCommLayer& tls)
{
	LOGI("Logging driver's query...");

	std::string msgBuf;

	tls.ReceiveMsg(connection, msgBuf);
	std::unique_ptr<ComMsg::DriQueryLog> queryLog = ParseMsg<ComMsg::DriQueryLog>(msgBuf);

	const ComMsg::Point2D<double>& loc = queryLog->GetLoc();

	PRINT_I("Logged Driver Query:");
	PRINT_I("Driver ID:\n%s", queryLog->GetDriverId().c_str());
	PRINT_I("Location: (%f, %f)", loc.GetX(), loc.GetY());

}

static void RequestPaymentInfo(void* const connection, Decent::Net::TlsCommLayer& tls)
{
	LOGI("Processing payment info request...");

	std::string driId;
	tls.ReceiveMsg(connection, driId);

	LOGI("Looking for payment info of driver:\n %s", driId.c_str());

	std::string driPayInfo;
	std::string selfPayInfo = OperatorPayment::GetPaymentInfo();
	{
		std::unique_lock<std::mutex> profilesLock(gs_profileMapMutex);
		auto it = gsk_profileMap.find(driId);
		if (it == gsk_profileMap.cend())
		{
			return;
		}
		driPayInfo = it->second.m_pay;
	}

	LOGI("Driver profile is found. Replying payment info...");

	ComMsg::RequestedPayment payInfo(std::move(driPayInfo), std::move(selfPayInfo));

	tls.SendMsg(connection, payInfo.ToString());
}

extern "C" int ecall_ride_share_dm_from_dri(void* const connection)
{
	if (!OperatorPayment::IsPaymentInfoValid())
	{
		return false;
	}

	using namespace EncFunc::DriverMgm;

	LOGI("Processing message from driver...");

	try
	{
		std::shared_ptr<TlsConfigWithName> tlsCfg = std::make_shared<TlsConfigWithName>(gs_state, TlsConfig::Mode::ServerNoVerifyPeer, "NaN");
		Decent::Net::TlsCommLayer tls(connection, tlsCfg, false);

		NumType funcNum;
		tls.ReceiveStruct(connection, funcNum);

		switch (funcNum)
		{
		case k_userReg:
			ProcessDriRegisterReq(connection, tls);
			break;
		default:
			break;
		}
	}
	catch (const std::exception& e)
	{
		PRINT_W("Failed to processing message from driver. Caught exception: %s", e.what());
	}

	return false;
}

extern "C" int ecall_ride_share_dm_from_trip_matcher(void* const connection)
{
	if (!OperatorPayment::IsPaymentInfoValid())
	{
		return false;
	}

	using namespace EncFunc::DriverMgm;

	LOGI("Processing message from Trip Matcher...");

	try
	{
		std::shared_ptr<TlsConfigWithName> tlsCfg = std::make_shared<TlsConfigWithName>(gs_state, TlsConfig::Mode::ServerVerifyPeer, AppNames::sk_tripMatcher);
		Decent::Net::TlsCommLayer tls(connection, tlsCfg, true);

		NumType funcNum;
		tls.ReceiveStruct(connection, funcNum);

		switch (funcNum)
		{
		case k_logQuery:
			LogQuery(connection, tls);
			break;
		default:
			break;
		}
	}
	catch (const std::exception& e)
	{
		PRINT_W("Failed to processing message from Trip Matcher. Caught exception: %s", e.what());
	}
	

	return false;
}

extern "C" int ecall_ride_share_dm_from_payment(void* const connection)
{
	if (!OperatorPayment::IsPaymentInfoValid())
	{
		return false;
	}

	using namespace EncFunc::DriverMgm;

	LOGI("Processing message from Payment Services...");

	try
	{
		std::shared_ptr<TlsConfigWithName> tlsCfg = std::make_shared<TlsConfigWithName>(gs_state, TlsConfig::Mode::ServerVerifyPeer, AppNames::sk_payment);
		Decent::Net::TlsCommLayer tls(connection, tlsCfg, true);

		NumType funcNum;
		tls.ReceiveStruct(connection, funcNum);

		switch (funcNum)
		{
		case k_getPayInfo:
			RequestPaymentInfo(connection, tls);
			break;
		default:
			break;
		}
	}
	catch (const std::exception& e)
	{
		PRINT_W("Failed to processing message from Payment Services. Caught exception: %s", e.what());
	}

	return false;
}
