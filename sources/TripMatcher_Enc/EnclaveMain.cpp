#include <map>
#include <memory>
#include <mutex>
#include <algorithm>
#include <condition_variable>

#include <DecentApi/Common/Common.h>
#include <DecentApi/Common/make_unique.h>
#include <DecentApi/Common/Ra/ClientX509Cert.h>
#include <DecentApi/Common/Ra/TlsConfigClient.h>
#include <DecentApi/Common/Ra/TlsConfigWithName.h>
#include <DecentApi/Common/Ra/KeyContainer.h>
#include <DecentApi/Common/Ra/CertContainer.h>
#include <DecentApi/Common/Net/TlsCommLayer.h>
#include <DecentApi/Common/Tools/JsonTools.h>
#include <DecentApi/Common/MbedTls/Hasher.h>

#include <DecentApi/CommonEnclave/Net/EnclaveConnectionOwner.h>
#include <DecentApi/CommonEnclave/Net/EnclaveCntTranslator.h>

#include <DecentApi/DecentAppEnclave/AppStatesSingleton.h>

#include <rapidjson/document.h>
#include <cppcodec/base64_default_rfc4648.hpp>

#include "../Common/AppNames.h"
#include "../Common/RideSharingFuncNums.h"
#include "../Common/RideSharingMessages.h"
#include "../Common/UnexpectedErrorException.h"

#include "../Common_Enc/OperatorPayment.h"

#include "Enclave_t.h"

using namespace RideShare;
using namespace Decent;
using namespace Decent::Ra;
using namespace Decent::Net;
using namespace Decent::Tools;

namespace
{
	static AppStates& gs_state = GetAppStateSingleton();
	
	struct ConfirmedQuoteItem
	{
		ComMsg::PasContact m_contact;
		ComMsg::Quote m_quote;
		std::string m_tripId;
		std::unique_ptr<ComMsg::DriContact> m_driContact;
		std::string m_driId;

		std::mutex m_mutex;
		std::condition_variable m_cond;

		ConfirmedQuoteItem(const ComMsg::PasContact& contact, const ComMsg::Quote& quote, const std::string& tripId) :
			m_contact(contact),
			m_quote(quote),
			m_tripId(tripId),
			m_driId()
		{}

		ConfirmedQuoteItem(const ConfirmedQuoteItem& rhs) = delete;
		ConfirmedQuoteItem(ConfirmedQuoteItem&& rhs) = delete;
	};

	struct MatchedItem
	{
		ComMsg::Quote m_quote;
		bool m_isEndByPas;
		bool m_isEndByDri;
		std::string m_driId;
		std::mutex m_mutex;

		MatchedItem(ComMsg::Quote&& quote, const std::string& driId) :
			m_quote(std::forward<ComMsg::Quote>(quote)),
			m_isEndByPas(false),
			m_isEndByDri(false),
			m_driId(driId)
		{}

		MatchedItem(ComMsg::Quote&& quote, std::string&& driId) :
			m_quote(std::forward<ComMsg::Quote>(quote)),
			m_isEndByPas(false),
			m_isEndByDri(false),
			m_driId(std::forward<std::string>(driId))
		{}
	};

	typedef std::map<ConfirmedQuoteItem*, std::unique_ptr<ConfirmedQuoteItem> > ConfirmedQuoteMapType;
	//Double as key should be fine here. B.C. we either find() using the original value, or compare() for a range.
	typedef std::map<double, std::vector<ConfirmedQuoteItem*> > ConfirmedQuotePosMapType;
	ConfirmedQuoteMapType gs_confirmedQuoteMap;
	//const ConfirmedQuoteMapType& gsk_confirmedQuoteMap = gs_confirmedQuoteMap;
	ConfirmedQuotePosMapType gs_confirmedQuotePosXMap;
	//const ConfirmedQuotePosMapType& gsk_confirmedQuotePosXMap = gs_confirmedQuotePosXMap;
	ConfirmedQuotePosMapType gs_confirmedQuotePosYMap;
	//const ConfirmedQuotePosMapType& gsk_confirmedQuotePosYMap = gs_confirmedQuotePosYMap;
	std::map<std::string, ConfirmedQuoteItem*> gs_confirmedQuoteIdMap;
	std::mutex gs_confirmedQuoteMapMutex;

	constexpr double gsk_distanceLimit = 10.0;
	constexpr size_t gsk_maxBestMatchSize = 5;

	typedef std::map<std::string, std::shared_ptr<MatchedItem> > MatchedMapType;
	MatchedMapType gs_matchedMap;
	const MatchedMapType& gsk_matchedMap = gs_matchedMap;
	std::mutex gs_matchedMapMutex;

	template<typename MsgType>
	static std::unique_ptr<MsgType> ParseMsg(const std::string& msgStr)
	{
		rapidjson::Document json;
		Decent::Tools::ParseStr2Json(json, msgStr);
		return Decent::Tools::make_unique<MsgType>(json);
	}
	
	static std::unique_ptr<ComMsg::SignedQuote> ParseSignedQuote(const std::string& msg, Decent::Ra::States& state)
	{
		JsonDoc json;
		ParseStr2Json(json, msg);
		return Decent::Tools::make_unique<ComMsg::SignedQuote>(ComMsg::SignedQuote::ParseSignedQuote(json, state, AppNames::sk_tripPlanner));
	}
}

static std::string ConstructTripId(const std::string& signedQuote)
{
	using namespace Decent::MbedTlsObj;

	General256Hash hash;
	Hasher<HashType::SHA256>().Calc(hash, signedQuote);

	return cppcodec::base64_rfc4648::encode(hash);
}

static bool AddConfirmedQuote(std::unique_ptr<ConfirmedQuoteItem>& item)
{
	ConfirmedQuoteItem* itemPtr = item.get();
	double x = item->m_quote.GetGetQuote().GetOri().GetX();
	double y = item->m_quote.GetGetQuote().GetOri().GetY();
	const std::string& tripId = item->m_tripId;

	std::unique_lock<std::mutex> mapLockQuote(gs_confirmedQuoteMapMutex);
	if (gs_confirmedQuoteIdMap.find(tripId) != gs_confirmedQuoteIdMap.end())
	{
		LOGI("Quote already exist!");
		return false;
	}

	{
		std::unique_lock<std::mutex> mapLockMatched(gs_matchedMapMutex);
		if (gsk_matchedMap.find(tripId) != gsk_matchedMap.cend())
		{
			LOGI("Quote already exist!");
			return false;
		}
	}
	gs_confirmedQuoteMap.insert(std::make_pair(itemPtr, std::move(item)));
	gs_confirmedQuotePosXMap[x].push_back(itemPtr);
	gs_confirmedQuotePosYMap[y].push_back(itemPtr);
	gs_confirmedQuoteIdMap.insert(std::make_pair(tripId, itemPtr));

	return true;
}

static void RemoveConfirmedQuotePos(ConfirmedQuotePosMapType& map, double key, ConfirmedQuoteItem* const itemPtr)
{
	auto it = map.find(key);
	DEBUG_ASSERT(it != map.end());

	auto posIt = std::find(it->second.begin(), it->second.end(), itemPtr);
	DEBUG_ASSERT(posIt != it->second.end());
	
	it->second.erase(posIt);

	if (it->second.size() == 0)
	{
		map.erase(it);
	}
}

static std::unique_ptr<ConfirmedQuoteItem> RemoveConfirmedQuote(ConfirmedQuoteItem* const itemPtr)
{
	std::unique_lock<std::mutex> mapLock(gs_confirmedQuoteMapMutex);
	auto it = gs_confirmedQuoteMap.find(itemPtr);

	DEBUG_ASSERT(it != gs_confirmedQuoteMap.end());
	
	std::unique_ptr<ConfirmedQuoteItem> res = std::move(it->second);
	gs_confirmedQuoteMap.erase(it);

	double x = res->m_quote.GetGetQuote().GetOri().GetX();
	double y = res->m_quote.GetGetQuote().GetOri().GetY();

	RemoveConfirmedQuotePos(gs_confirmedQuotePosXMap, x, itemPtr);
	RemoveConfirmedQuotePos(gs_confirmedQuotePosYMap, y, itemPtr);

	return std::move(res);
}

static void AddMatchedItem(const std::string& tripId, std::shared_ptr<MatchedItem>& matched)
{
	std::unique_lock<std::mutex> mapLock(gs_matchedMapMutex);
	DEBUG_ASSERT(gsk_matchedMap.find(tripId) == gsk_matchedMap.cend());
	
	gs_matchedMap.insert(std::make_pair(tripId, std::move(matched)));

	{
		std::unique_lock<std::mutex> mapLockQuote(gs_confirmedQuoteMapMutex);
		gs_confirmedQuoteIdMap.erase(tripId);
	}
}

static bool VerifyContactHash(const std::string& certPem, const Decent::General256Hash& contactHash)
{
	ClientX509Cert cert(certPem);

	return cert.GetAppId() == cppcodec::base64_rfc4648::encode(contactHash);
}

static inline std::string GetClientIdFromTls(const Decent::Net::TlsCommLayer& tls)
{
	return tls.GetPublicKeyPem();
}

static void ProcessPasConfirmQuoteReq(void* const connection, Decent::Net::TlsCommLayer& tls)
{
	LOGI("Processing passenger confirm request...");

	EnclaveCntTranslator cnt(connection);

	const std::string pasId = GetClientIdFromTls(tls);

	std::string msgBuf;

	msgBuf = tls.RecvContainer<std::string>(cnt);
	std::unique_ptr<ComMsg::ConfirmQuote> confirmQuote = ParseMsg<ComMsg::ConfirmQuote>(msgBuf);
	std::unique_ptr<ComMsg::SignedQuote> signedQuote = ParseSignedQuote(confirmQuote->GetSignQuote(), gs_state);
	std::unique_ptr<ComMsg::Quote> quote = ParseMsg<ComMsg::Quote>(signedQuote->GetQuote());

	if (quote->GetPasId() != pasId)
	{
		return;
	}

	//verify pas contact.
	if (!VerifyContactHash(tls.GetPeerCertPem(), confirmQuote->GetContact().CalcHash()))
	{
		LOGW("Passenger's contact doesn't match!");
		return;
	}

	std::string tripId = ConstructTripId(confirmQuote->GetSignQuote());
	PRINT_I("Got quote with trip ID: %s.", tripId.c_str());
	std::unique_ptr<ConfirmedQuoteItem> item = make_unique<ConfirmedQuoteItem>(confirmQuote->GetContact(), *quote, tripId);
	ConfirmedQuoteItem* itemPtr = item.get();

	//Clean up memory:
	confirmQuote.reset();
	signedQuote.reset();
	quote.reset();

	std::mutex& itemMutex = item->m_mutex;
	std::condition_variable& itemCond = item->m_cond;
	const std::unique_ptr<ComMsg::DriContact>& driContact = item->m_driContact;

	std::unique_lock<std::mutex> itemLock(itemMutex);
	if (!AddConfirmedQuote(item))
	{
		return;
	}
	item.reset();

	PRINT_I("Waiting for a match for the trip with ID %s...", tripId.c_str());
	itemCond.wait(itemLock, [&driContact] {return static_cast<bool>(driContact); });
	PRINT_I("Match for the trip with ID %s is found.", tripId.c_str());

	item = RemoveConfirmedQuote(itemPtr);

	std::shared_ptr<MatchedItem> matched = std::make_shared<MatchedItem>(std::move(item->m_quote), std::move(item->m_driId));

	AddMatchedItem(item->m_tripId, matched);

	ComMsg::PasMatchedResult pasMatchedRes(std::move(item->m_tripId), std::move(*item->m_driContact));
	item.reset();

	tls.SendContainer(cnt, pasMatchedRes.ToString());

}

static void TripStart(void* const connection, Decent::Net::TlsCommLayer& tls, const bool isPassenger)
{
	LOGI("Processing trip start from %s...", isPassenger ? "passenger" : "driver");

	EnclaveCntTranslator cnt(connection);

	std::string tripId;

	tripId = tls.RecvContainer<std::string>(cnt);

	std::shared_ptr<MatchedItem> item;
	{
		//Check if it's in the map.
		std::unique_lock<std::mutex> mapLock(gs_matchedMapMutex);
		auto it = gs_matchedMap.find(tripId);
		if (it != gs_matchedMap.end())
		{
			item = it->second;
		}
	}
	LOGI("The trip is %s. The ID is %s.", item ? "found" : "not found", tripId.c_str());

	if (!item ||
		GetClientIdFromTls(tls) != (isPassenger ? item->m_quote.GetPasId() : item->m_driId))
	{
		LOGI("Requester is not the %s of this trip!", isPassenger ? "passenger" : "driver");
		return;
	}
}

static void ProcessPayment(const std::shared_ptr<MatchedItem>& item)
{
	using namespace EncFunc::Payment;

	ComMsg::FinalBill bill(std::move(item->m_quote), std::string(OperatorPayment::GetPaymentInfo()), std::move(item->m_driId));

	LOGI("Sending final bill to payment services...");
	EnclaveConnectionOwner cnt = EnclaveConnectionOwner::CntBuilder(SGX_SUCCESS, &ocall_ride_share_cnt_mgr_get_payment);

	std::shared_ptr<TlsConfigWithName> tlsCfg = std::make_shared<TlsConfigWithName>(gs_state, TlsConfigWithName::Mode::ClientHasCert, AppNames::sk_payment, nullptr);
	Decent::Net::TlsCommLayer tls(cnt, tlsCfg, true, nullptr);

	tls.SendStruct(cnt, k_procPayment);
	tls.SendContainer(cnt, bill.ToString());
}

static void TripEnd(void* const connection, Decent::Net::TlsCommLayer& tls, const bool isPassenger)
{
	LOGI("Processing trip end from %s...", isPassenger ? "passenger" : "driver");

	EnclaveCntTranslator cnt(connection);

	std::string tripId = tls.RecvContainer<std::string>(cnt);

	std::shared_ptr<MatchedItem> item;
	{
		//Check if it's in the map.
		std::unique_lock<std::mutex> mapLock(gs_matchedMapMutex);
		auto it = gs_matchedMap.find(tripId);
		if (it != gs_matchedMap.end())
		{
			item = it->second;
		}
	}

	LOGI("The trip is %s. The ID is %s.", item ? "found" : "not found", tripId.c_str());

	if (!item ||
		GetClientIdFromTls(tls) != (isPassenger ? item->m_quote.GetPasId() : item->m_driId))
	{
		LOGI("Requester is not the %s of this trip!", isPassenger ? "passenger" : "driver");
		return;
	}

	std::unique_lock<std::mutex> itemLock(item->m_mutex);

	if (item->m_isEndByPas && item->m_isEndByDri) //Trip is already finished by other process.
	{
		return;
	}

	switch (isPassenger)
	{
	case false: //zero
		item->m_isEndByDri = true;
		break;
	default: //true, non-zero
		item->m_isEndByPas = true;
		break;
	}

	if (item->m_isEndByPas && item->m_isEndByDri) //Trip is finished.
	{
		{//Remove it from map first.
			std::unique_lock<std::mutex> mapLock(gs_matchedMapMutex);
			auto it = gs_matchedMap.find(tripId);
			if (it != gs_matchedMap.end())
			{
				gs_matchedMap.erase(it);
			}
		}
		
		ProcessPayment(item); //Caution: m_quote & m_driId in item become invalid after this call!
	}
}

static std::vector<std::pair<ConfirmedQuoteItem*, double> > FindMatchInitial(const ComMsg::Point2D<double>& driLoc)
{
	std::unique_lock<std::mutex> mapLock(gs_confirmedQuoteMapMutex);

	std::map<ConfirmedQuoteItem*, double> midRes1;
	{
		auto end = gs_confirmedQuotePosXMap.upper_bound(driLoc.GetX() + gsk_distanceLimit);
		for (auto it = gs_confirmedQuotePosXMap.lower_bound(driLoc.GetX() - gsk_distanceLimit);
			it != end; ++it)
		{
			double dist = pow(driLoc.GetX() - it->first, 2);

			for (ConfirmedQuoteItem* ptr : it->second)
			{
				midRes1[ptr] = dist;
			}
		}
	}

	std::vector<std::pair<ConfirmedQuoteItem*, double> > midRes2;
	{
		auto end = gs_confirmedQuotePosYMap.upper_bound(driLoc.GetY() + gsk_distanceLimit);
		for (auto it = gs_confirmedQuotePosYMap.lower_bound(driLoc.GetY() - gsk_distanceLimit);
			it != end; ++it)
		{
			double dist = pow(driLoc.GetY() - it->first, 2);

			for (ConfirmedQuoteItem* ptr : it->second)
			{
				auto midResIt = midRes1.find(ptr);
				if (midResIt != midRes1.end())
				{
					midRes2.push_back(std::make_pair(ptr, sqrt(midResIt->second + dist)));
				}
			}
		}
	}

	return std::move(midRes2);
}

static std::vector<ConfirmedQuoteItem*> FindMatchSort(std::vector<std::pair<ConfirmedQuoteItem*, double> >& midRes)
{
	std::vector<ConfirmedQuoteItem*> res;
	res.reserve(midRes.size());

	std::sort(midRes.begin(), midRes.end(), [](const std::pair<ConfirmedQuoteItem*, double>& a, const std::pair<ConfirmedQuoteItem*, double>&b)
	{
		return a.second < b.second;
	});

	LOGI("List of matches:");
	for (const std::pair<ConfirmedQuoteItem*, double>& item : midRes)
	{
		LOGI("\tDist: %f", item.second);
		res.push_back(item.first);
	}

	return std::move(res);
}

static std::vector<ComMsg::MatchItem> FindMatchFinal(const std::vector<ConfirmedQuoteItem*>& matchesList)
{
	std::vector<ComMsg::MatchItem> res;

	std::unique_lock<std::mutex> mapLock(gs_confirmedQuoteMapMutex);

	for (size_t i = 0; i < matchesList.size() && i < gsk_maxBestMatchSize; ++i)
	{
		auto it = gs_confirmedQuoteMap.find(matchesList[i]);
		if (it != gs_confirmedQuoteMap.end())
		{
			res.push_back(ComMsg::MatchItem(it->second->m_tripId, it->second->m_quote.GetPath()));
		}
	}

	return std::move(res);
}

static bool SendQueryLog(const std::string& userId, const ComMsg::Point2D<double>& loc)
{
	using namespace EncFunc::DriverMgm;

	LOGI("Sending query log to a driver management...");

	EnclaveConnectionOwner cnt = EnclaveConnectionOwner::CntBuilder(SGX_SUCCESS, &ocall_ride_share_cnt_mgr_get_dri_mgm);

	ComMsg::DriQueryLog log(userId, loc);

	std::shared_ptr<TlsConfigWithName> tlsCfg = std::make_shared<TlsConfigWithName>(gs_state, TlsConfigWithName::Mode::ClientHasCert, AppNames::sk_driverMgm, nullptr);
	Decent::Net::TlsCommLayer tls(cnt, tlsCfg, true, nullptr);

	tls.SendStruct(cnt, k_logQuery);
	tls.SendContainer(cnt, log.ToString());

	return true;
}

static void DriverFindMatchReq(void* const connection, Decent::Net::TlsCommLayer& tls)
{
	LOGI("Process driver find match request...");

	EnclaveCntTranslator cnt(connection);

	std::string msgBuf = tls.RecvContainer<std::string>(cnt);
	std::unique_ptr<ComMsg::DriverLoc> driLoc = ParseMsg<ComMsg::DriverLoc>(msgBuf);

	const std::string driId = tls.GetPublicKeyPem();
	if (driId.size() == 0 ||
		!SendQueryLog(driId, driLoc->GetLoc()))
	{
		return;
	}
	
	std::vector<std::pair<ConfirmedQuoteItem*, double> > midRes = FindMatchInitial(driLoc->GetLoc());
	const std::vector<ConfirmedQuoteItem*> matchesList = FindMatchSort(midRes);

	const ComMsg::BestMatches bestMatches = FindMatchFinal(matchesList);

	tls.SendContainer(cnt, bestMatches.ToString());
}

static void DriverConfirmMatchReq(void* const connection, Decent::Net::TlsCommLayer& tls)
{
	LOGI("Process driver confirm match request...");

	EnclaveCntTranslator cnt(connection);

	const std::string driId = GetClientIdFromTls(tls);

	std::string msgBuf = tls.RecvContainer<std::string>(cnt);
	std::unique_ptr<ComMsg::DriSelection> selection = ParseMsg<ComMsg::DriSelection>(msgBuf);

	//verify pas contact.
	if (!VerifyContactHash(tls.GetPeerCertPem(), selection->GetContact().CalcHash()))
	{
		LOGI("Driver's contact doesn't match!");
		return;
	}

	std::unique_ptr<ComMsg::PasContact> pasContact;
	{
		std::unique_lock<std::mutex> mapLock(gs_confirmedQuoteMapMutex);

		auto idMapIt = gs_confirmedQuoteIdMap.find(selection->GetTripId());
		if (idMapIt == gs_confirmedQuoteIdMap.end())
		{
			return;
		}

		auto quoteMapIt = gs_confirmedQuoteMap.find(idMapIt->second);
		if (quoteMapIt == gs_confirmedQuoteMap.end() || 
			quoteMapIt->second->m_driContact)
		{
			return;
		}

		std::unique_ptr<ConfirmedQuoteItem>& item = quoteMapIt->second;
		std::unique_lock<std::mutex> itemLock(item->m_mutex);

		item->m_driContact = Tools::make_unique<ComMsg::DriContact>(std::move(selection->GetContact()));
		item->m_driId = driId;

		pasContact = Tools::make_unique<ComMsg::PasContact>(item->m_contact);

		item->m_cond.notify_one();
	}

	selection.reset();

	tls.SendContainer(cnt, pasContact->ToString());
}

extern "C" int ecall_ride_share_tm_from_pas(void* const connection)
{
	if (!OperatorPayment::IsPaymentInfoValid())
	{
		return false;
	}

	using namespace EncFunc::TripMatcher;

	LOGI("Processing message from passenger...");

	EnclaveCntTranslator cnt(connection);

	try
	{
		std::shared_ptr<TlsConfigClient> tlsCfg = std::make_shared<TlsConfigClient>(gs_state, TlsConfigClient::Mode::ServerVerifyPeer, AppNames::sk_passengerMgm, nullptr);
		TlsCommLayer tls(cnt, tlsCfg, true, nullptr);

		NumType funcNum;
		tls.RecvStruct(funcNum);

		switch (funcNum)
		{
		case k_confirmQuote:
			ProcessPasConfirmQuoteReq(connection, tls);
			break;
		case k_tripStart:
			TripStart(connection, tls, true);
			break;
		case k_tripEnd:
			TripEnd(connection, tls, true);
			break;
		default:
			break;
		}
	}
	catch (const std::exception& e)
	{
		PRINT_W("Failed to processing message from passenger. Caught exception: %s", e.what());
	}

	return false;
}

extern "C" int ecall_ride_share_tm_from_dri(void* const connection)
{
	if (!OperatorPayment::IsPaymentInfoValid())
	{
		return false;
	}

	using namespace EncFunc::TripMatcher;

	LOGI("Processing message from driver...");

	EnclaveCntTranslator cnt(connection);

	try
	{
		std::shared_ptr<TlsConfigClient> tlsCfg = std::make_shared<TlsConfigClient>(gs_state, TlsConfigClient::Mode::ServerVerifyPeer, AppNames::sk_driverMgm, nullptr);
		TlsCommLayer tls(cnt, tlsCfg, true, nullptr);

		NumType funcNum;
		tls.RecvStruct(funcNum);

		switch (funcNum)
		{
		case k_findMatch:
			DriverFindMatchReq(connection, tls);
			break;
		case k_confirmMatch:
			DriverConfirmMatchReq(connection, tls);
			break;
		case k_tripStart:
			TripStart(connection, tls, false);
			break;
		case k_tripEnd:
			TripEnd(connection, tls, false);
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
