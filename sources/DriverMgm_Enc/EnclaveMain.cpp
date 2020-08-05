#include <mutex>
#include <memory>
#include <map>

#include <DecentApi/Common/Common.h>
#include <DecentApi/Common/make_unique.h>
#include <DecentApi/Common/Ra/ClientX509Cert.h>
#include <DecentApi/Common/Ra/TlsConfigWithName.h>
#include <DecentApi/Common/Ra/KeyContainer.h>
#include <DecentApi/Common/Ra/CertContainer.h>
#include <DecentApi/Common/Net/TlsCommLayer.h>
#include <DecentApi/Common/Tools/JsonTools.h>
#include <DecentApi/Common/MbedTls/Drbg.h>
#include <DecentApi/Common/MbedTls/EcKey.h>
#include <DecentApi/Common/MbedTls/X509Req.h>
#include <DecentApi/CommonEnclave/Net/EnclaveCntTranslator.h>
#include <DecentApi/DecentAppEnclave/AppStatesSingleton.h>

#include <rapidjson/document.h>
#include <cppcodec/base64_default_rfc4648.hpp>

#include "../Common/AppNames.h"
#include "../Common/RideSharingFuncNums.h"
#include "../Common/RideSharingMessages.h"

#include "../Common_Enc/OperatorPayment.h"

using namespace RideShare;
using namespace Decent::Ra;
using namespace Decent::Net;

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
	using namespace Decent::MbedTlsObj;

	LOGI("Processing Driver Register Request...");

	EnclaveCntTranslator cnt(connection);

	std::string msgBuf = tls.RecvContainer<std::string>(cnt);
	std::unique_ptr<ComMsg::DriReg> driRegInfo = ParseMsg<ComMsg::DriReg>(msgBuf);

	if (!VerifyContactInfo(driRegInfo->GetContact()) ||
		!VerifyDriverLicense(driRegInfo->GetDriLic()) ||
		!VerifyPayment(driRegInfo->GetPayment()) )
	{
		return;
	}

	X509Req certReq = driRegInfo->GetCsr();
	certReq.VerifySignature();

	EcPublicKey<EcKeyType::SECP256R1> clientKey(EcPublicKey<EcKeyType::SECP256R1>(certReq.GetPublicKey()));
	std::string driId = clientKey.GetPublicPem();

	{
		std::unique_lock<std::mutex> profileMapLock(gs_profileMapMutex);
		if (gsk_profileMap.find(driId) != gsk_profileMap.cend())
		{
			LOGI("Client profile already exist.");
			return;
		}
	}
	std::shared_ptr<const AppX509Cert> cert = gs_state.GetAppCertContainer().GetAppCert();

	if (!cert)
	{
		LOGW("Decent App private key and certificate hasn't been initialized yet.");
		return;
	}

	EcKeyPair<EcKeyType::SECP256R1> prvKey = EcKeyPair<EcKeyType::SECP256R1>(*(gs_state.GetKeyContainer().GetSignKeyPair()));

	const ComMsg::DriContact& contact = driRegInfo->GetContact();
	ClientX509CertWriter clientCert(clientKey, *cert, prvKey, "Decent_RideShare_Driver",
		cppcodec::base64_rfc4648::encode(contact.CalcHash()));

	{
		std::unique_lock<std::mutex> pasProfilesLock(gs_profileMapMutex);

		gs_profileMap.insert(std::make_pair(driId,
			DriProfileItem(contact, driRegInfo->GetPayment(), driRegInfo->GetDriLic())));

		LOGI("Client profile added. Profile store size: %llu.", gsk_profileMap.size());
	}

	Drbg drbg;
	tls.SendContainer(cnt, clientCert.GeneratePemChain(drbg));

}

static void LogQuery(void* const connection, Decent::Net::TlsCommLayer& tls)
{
	LOGI("Logging driver's query...");

	EnclaveCntTranslator cnt(connection);

	std::string msgBuf = tls.RecvContainer<std::string>(cnt);
	std::unique_ptr<ComMsg::DriQueryLog> queryLog = ParseMsg<ComMsg::DriQueryLog>(msgBuf);

	const ComMsg::Point2D<double>& loc = queryLog->GetLoc();

	PRINT_I("Logged Driver Query:");
	PRINT_I("Driver ID:\n%s", queryLog->GetDriverId().c_str());
	PRINT_I("Location: (%f, %f)", loc.GetX(), loc.GetY());

}

static void RequestPaymentInfo(void* const connection, Decent::Net::TlsCommLayer& tls)
{
	LOGI("Processing payment info request...");

	EnclaveCntTranslator cnt(connection);

	std::string driId = tls.RecvContainer<std::string>(cnt);

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

	tls.SendContainer(cnt, payInfo.ToString());
}

extern "C" int ecall_ride_share_dm_from_dri(void* const connection)
{
	if (!OperatorPayment::IsPaymentInfoValid())
	{
		return false;
	}

	using namespace EncFunc::DriverMgm;

	LOGI("Processing message from driver...");

	EnclaveCntTranslator cnt(connection);

	try
	{
		std::shared_ptr<TlsConfigWithName> tlsCfg = std::make_shared<TlsConfigWithName>(gs_state, TlsConfigWithName::Mode::ServerNoVerifyPeer, "NaN", nullptr);
		Decent::Net::TlsCommLayer tls(cnt, tlsCfg, false, nullptr);

		NumType funcNum;
		tls.RecvStruct(cnt, funcNum);

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

	EnclaveCntTranslator cnt(connection);

	try
	{
		std::shared_ptr<TlsConfigWithName> tlsCfg = std::make_shared<TlsConfigWithName>(gs_state, TlsConfigWithName::Mode::ServerVerifyPeer, AppNames::sk_tripMatcher, nullptr);
		Decent::Net::TlsCommLayer tls(cnt, tlsCfg, true, nullptr);

		NumType funcNum;
		tls.RecvStruct(cnt, funcNum);

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

	EnclaveCntTranslator cnt(connection);

	try
	{
		std::shared_ptr<TlsConfigWithName> tlsCfg = std::make_shared<TlsConfigWithName>(gs_state, TlsConfigWithName::Mode::ServerVerifyPeer, AppNames::sk_payment, nullptr);
		Decent::Net::TlsCommLayer tls(cnt, tlsCfg, true, nullptr);

		NumType funcNum;
		tls.RecvStruct(cnt, funcNum);

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
