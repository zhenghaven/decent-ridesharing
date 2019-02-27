#include <cmath>

#include <memory>
#include <mutex>
#include <algorithm>
#include <condition_variable>

#include <DecentApi/Common/Common.h>
#include <DecentApi/Common/make_unique.h>
#include <DecentApi/Common/Ra/TlsConfig.h>
#include <DecentApi/Common/Ra/States.h>
#include <DecentApi/Common/Ra/KeyContainer.h>
#include <DecentApi/Common/Ra/CertContainer.h>
#include <DecentApi/Common/Net/TlsCommLayer.h>
#include <DecentApi/Common/Tools/JsonTools.h>
#include <DecentApi/Common/MbedTls/MbedTlsHelpers.h>

#include <rapidjson/document.h>
#include <cppcodec/base64_default_rfc4648.hpp>

#include "../Common/Crypto.h"
#include "../Common/TlsConfig.h"
#include "../Common/AppNames.h"
#include "../Common/RideSharingFuncNums.h"
#include "../Common/RideSharingMessages.h"
#include "../Common/UnexpectedErrorException.h"

using namespace RideShare;
using namespace Decent;
using namespace Decent::Ra;
using namespace Decent::Tools;

namespace
{
	static States& gs_state = States::Get();
	
	std::string gs_selfPaymentInfo = "Trip Matcher Payment Info Test";
	
	struct ConfirmedQuoteItem
	{
		ComMsg::PasContact m_contact;
		ComMsg::Quote m_quote;
		std::string m_tripId;
		std::mutex m_mutex;
		std::condition_variable m_cond;
		std::unique_ptr<ComMsg::DriContact> m_driContact;

		ConfirmedQuoteItem(const ComMsg::PasContact& contact, const ComMsg::Quote& quote, const std::string& tripId) :
			m_contact(contact),
			m_quote(quote),
			m_tripId(tripId)
		{}

		ConfirmedQuoteItem(const ConfirmedQuoteItem& rhs) = delete;
		ConfirmedQuoteItem(ConfirmedQuoteItem&& rhs) = delete;
	};

	struct MatchedItem
	{
		ComMsg::Quote m_quote;
		std::mutex m_mutex;

		MatchedItem(ComMsg::Quote&& quote) :
			m_quote(std::forward<ComMsg::Quote>(quote))
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
		//LOGW("Parsing Msg (size = %llu): \n%s\n", msgStr.size(), msgStr.c_str());
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

static std::unique_ptr<ComMsg::SignedQuote> ParseSignedQuote(const std::string& msg, Decent::Ra::States& state)
{
	try
	{
		JsonDoc json;
		return ParseStr2Json(json, msg) ? 
			Decent::Tools::make_unique<ComMsg::SignedQuote>(ComMsg::SignedQuote::ParseSignedQuote(json, state, AppNames::sk_tripPlanner)) :
			nullptr;
	}
	catch (const std::exception&)
	{
		LOGW("Failed to parse SignedQuote message!")
		return nullptr;
	}
}

static std::string ConstructTripId(const std::string& signedQuote)
{
	General256Hash hash;
	MbedTlsHelper::CalcHashSha256(signedQuote, hash);

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
		LOGW("Quote already exist!");
		return false;
	}

	{
		std::unique_lock<std::mutex> mapLockMatched(gs_matchedMapMutex);
		if (gsk_matchedMap.find(tripId) != gsk_matchedMap.cend())
		{
			LOGW("Quote already exist!");
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

static void ProcessPasConfirmQuoteReq(void* const connection, Decent::Net::TlsCommLayer& tls)
{
	LOGI("Processing passenger confirm request...");

	std::string msgBuf;

	std::unique_ptr<ComMsg::ConfirmQuote> confirmQuote;
	std::unique_ptr<ComMsg::SignedQuote> signedQuote;
	std::unique_ptr<ComMsg::Quote> quote;
	if (!tls.ReceiveMsg(connection, msgBuf) ||
		!(confirmQuote = ParseMsg<ComMsg::ConfirmQuote>(msgBuf)) ||
		!(signedQuote = ParseSignedQuote(confirmQuote->GetSignQuote(), gs_state)) ||
		!(quote = ParseMsg<ComMsg::Quote>(signedQuote->GetQuote())) )
	{
		return;
	}

	/*TODO: verify pas contact.*/

	std::string tripId = ConstructTripId(confirmQuote->GetSignQuote());
	LOGI("Got quote with trip ID: %s.", tripId.c_str());
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

	itemCond.wait(itemLock, [&driContact] {return static_cast<bool>(driContact); });
	LOGI("Match for the trip with ID %s is found.", tripId.c_str());

	item = RemoveConfirmedQuote(itemPtr);

	std::shared_ptr<MatchedItem> matched = std::make_shared<MatchedItem>(std::move(item->m_quote));

	AddMatchedItem(item->m_tripId, matched);

	ComMsg::PasMatchedResult pasMatchedRes(std::move(item->m_tripId), std::move(*item->m_driContact));
	item.reset();

	tls.SendMsg(connection, pasMatchedRes.ToString());

}

extern "C" int ecall_ride_share_tm_from_pas(void* const connection)
{
	using namespace EncFunc::TripMatcher;

	LOGI("Processing message from passenger...");

	std::shared_ptr<RideShare::TlsConfig> tlsCfg = std::make_shared<RideShare::TlsConfig>(AppNames::sk_passengerMgm, gs_state, true);
	Decent::Net::TlsCommLayer pasTls(connection, tlsCfg, true);

	std::string msgBuf;
	if (!pasTls.ReceiveMsg(connection, msgBuf) ||
		msgBuf.size() != sizeof(NumType))
	{
		LOGI("Recv size: %llu", msgBuf.size());
		return false;
	}

	LOGI("TLS Handshake Successful!");

	const NumType funcNum = EncFunc::GetNum<EncFunc::PassengerMgm::NumType>(msgBuf);

	LOGI("Request Function: %d.", funcNum);

	switch (funcNum)
	{
	case k_confirmQuote:
		ProcessPasConfirmQuoteReq(connection, pasTls);
		break;
	case k_tripStart:
	case k_tripEnd:
	default:
		break;
	}

	return false;
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
		LOGI("\t%#X, %f", item.first, item.second);
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

static void DriverFindMatchReq(void* const connection, Decent::Net::TlsCommLayer& tls)
{
	LOGI("Process driver find match request...");

	std::string msgBuf;

	std::unique_ptr<ComMsg::DriverLoc> driLoc;
	if (!tls.ReceiveMsg(connection, msgBuf) ||
		!(driLoc = ParseMsg<ComMsg::DriverLoc>(msgBuf)))
	{
		return;
	}

	//TODO: Send Query Log
	
	std::vector<std::pair<ConfirmedQuoteItem*, double> > midRes = FindMatchInitial(driLoc->GetLoc());
	const std::vector<ConfirmedQuoteItem*> matchesList = FindMatchSort(midRes);

	const ComMsg::BestMatches bestMatches = FindMatchFinal(matchesList);

	tls.SendMsg(connection, bestMatches.ToString());
}

static void DriverConfirmMatchReq(void* const connection, Decent::Net::TlsCommLayer& tls)
{
	LOGI("Process driver confirm match request...");

	std::string msgBuf;

	std::unique_ptr<ComMsg::DriSelection> selection;
	if (!tls.ReceiveMsg(connection, msgBuf) ||
		!(selection = ParseMsg<ComMsg::DriSelection>(msgBuf)))
	{
		return;
	}

	/*TODO: Verify Driver contact. */

	std::unique_ptr<ComMsg::PasContact> pasContact;
	{
		std::unique_lock<std::mutex> mapLock(gs_confirmedQuoteMapMutex);

		auto idMapIt = gs_confirmedQuoteIdMap.find(selection->GetTripId().GetId());
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

		item->m_driContact = Tools::make_unique<ComMsg::DriContact>(std::move(selection->GetContact()));

		pasContact = Tools::make_unique<ComMsg::PasContact>(item->m_contact);

		item->m_cond.notify_one();
	}

	selection.reset();

	tls.SendMsg(connection, pasContact->ToString());
}

extern "C" int ecall_ride_share_tm_from_dri(void* const connection)
{
	using namespace EncFunc::TripMatcher;

	LOGI("Processing message from driver...");

	std::shared_ptr<RideShare::TlsConfig> tlsCfg = std::make_shared<RideShare::TlsConfig>(AppNames::sk_driverMgm, gs_state, true);
	Decent::Net::TlsCommLayer driTls(connection, tlsCfg, true);

	std::string msgBuf;
	if (!driTls.ReceiveMsg(connection, msgBuf) ||
		msgBuf.size() != sizeof(NumType))
	{
		LOGW("Recv size: %llu", msgBuf.size());
		return false;
	}

	LOGI("TLS Handshake Successful!");

	const NumType funcNum = EncFunc::GetNum<NumType>(msgBuf);

	LOGI("Request Function: %d.", funcNum);

	switch (funcNum)
	{
	case k_findMatch:
		DriverFindMatchReq(connection, driTls);
		break;
	case k_confirmMatch:
		DriverConfirmMatchReq(connection, driTls);
		break;
	case k_tripStart:
	case k_tripEnd:
	default:
		break;
	}

	return false;
}
