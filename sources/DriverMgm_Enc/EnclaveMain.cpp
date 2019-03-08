#include <mutex>
#include <memory>

#include <DecentApi/Common/Common.h>
#include <DecentApi/Common/make_unique.h>
#include <DecentApi/Common/Ra/TlsConfig.h>
#include <DecentApi/Common/Ra/KeyContainer.h>
#include <DecentApi/Common/Ra/CertContainer.h>
#include <DecentApi/Common/Net/TlsCommLayer.h>
#include <DecentApi/Common/Tools/JsonTools.h>
#include <DecentApi/DecentAppEnclave/AppStatesSingleton.h>

#include <rapidjson/document.h>
#include <cppcodec/base64_default_rfc4648.hpp>

#include "../Common/Crypto.h"
#include "../Common/TlsConfig.h"
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
		try
		{
			rapidjson::Document json;
			return Decent::Tools::ParseStr2Json(json, msgStr) ? 
				Decent::Tools::make_unique<MsgType>(json) :
				nullptr;
		}
		catch (const std::exception&)
		{
			return nullptr;
		}
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

	std::unique_ptr<ComMsg::DriReg> driRegInfo;

	tls.ReceiveMsg(connection, msgBuf);
	if (!(driRegInfo = ParseMsg<ComMsg::DriReg>(msgBuf)) )
	{
		return;
	}

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
	ClientX509 clientCert(certReq.GetEcPublicKey(), *cert, *prvKey, "Decent_RideShare_Driver_" + contact.GetName(),
		cppcodec::base64_rfc4648::encode(contact.CalcHash()));

	{
		std::unique_lock<std::mutex> pasProfilesLock(gs_profileMapMutex);

		gs_profileMap.insert(std::make_pair(pubPemStr,
			DriProfileItem(contact, driRegInfo->GetPayment(), driRegInfo->GetDriLic())));

		LOGI("Client profile added. Profile store size: %llu.", gsk_profileMap.size());
	}

	tls.SendMsg(connection, clientCert.ToPemString());

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
		std::shared_ptr<Decent::Ra::TlsConfig> driTlsCfg = std::make_shared<Decent::Ra::TlsConfig>("NaN", gs_state, true);
		Decent::Net::TlsCommLayer driTls(connection, driTlsCfg, false);

		NumType funcNum;
		driTls.ReceiveStruct(connection, funcNum);

		LOGI("Request Function: %d.", funcNum);

		switch (funcNum)
		{
		case k_userReg:
			ProcessDriRegisterReq(connection, driTls);
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

static void LogQuery(void* const connection, Decent::Net::TlsCommLayer& tls)
{
	LOGI("Logging driver's query...");

	std::string msgBuf;

	std::unique_ptr<ComMsg::DriQueryLog> queryLog;
	tls.ReceiveMsg(connection, msgBuf);
	if (!(queryLog = ParseMsg<ComMsg::DriQueryLog>(msgBuf)))
	{
		return;
	}

	const ComMsg::Point2D<double>& loc = queryLog->GetLoc();

	PRINT_I("Logged Driver Query:");
	PRINT_I("Driver ID:\n%s", queryLog->GetDriverId().c_str());
	PRINT_I("Location: (%f, %f)", loc.GetX(), loc.GetY());

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
		std::shared_ptr<Decent::Ra::TlsConfig> tmTlsCfg = std::make_shared<Decent::Ra::TlsConfig>(AppNames::sk_tripMatcher, gs_state, true);
		Decent::Net::TlsCommLayer tmTls(connection, tmTlsCfg, true);

		NumType funcNum;
		tmTls.ReceiveStruct(connection, funcNum);

		LOGI("Request Function: %d.", funcNum);

		switch (funcNum)
		{
		case k_logQuery:
			LogQuery(connection, tmTls);
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
		std::shared_ptr<Decent::Ra::TlsConfig> payTlsCfg = std::make_shared<Decent::Ra::TlsConfig>(AppNames::sk_payment, gs_state, true);
		Decent::Net::TlsCommLayer payTls(connection, payTlsCfg, true);

		NumType funcNum;
		payTls.ReceiveStruct(connection, funcNum);

		LOGI("Request Function: %d.", funcNum);

		switch (funcNum)
		{
		case k_getPayInfo:
			RequestPaymentInfo(connection, payTls);
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
