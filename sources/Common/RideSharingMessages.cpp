#include "RideSharingMessages.h"

#ifdef ENCLAVE_ENVIRONMENT
#include <rapidjson/document.h>
#else
#include <json/json.h>
#endif

#include <mbedtls/md.h>
#include <mbedtls/x509_crt.h>

#include <DecentApi/Common/Common.h>
#include <DecentApi/Common/Tools/JsonTools.h>
#include <DecentApi/Common/Tools/DataCoding.h>
#include <DecentApi/Common/MbedTls/Hasher.h>
#include <DecentApi/Common/Ra/Crypto.h>
#include <DecentApi/Common/Ra/TlsConfigWithName.h>
#include <DecentApi/Common/Ra/States.h>
#include <DecentApi/Common/Ra/KeyContainer.h>
#include <DecentApi/Common/Ra/CertContainer.h>

#include "MessageException.h"

using namespace RideShare;
using namespace RideShare::ComMsg;
using namespace Decent;
using namespace Decent::Ra;

namespace
{
	template<typename T>
	std::vector<T> ParseArray(const JsonValue & json)
	{
		if (json.JSON_IS_ARRAY())
		{
			std::vector<T> res;
			for (auto it = json.JSON_ARR_BEGIN(); it != json.JSON_ARR_END(); ++it)
			{
				res.push_back(JSON_ARR_GETVALUE(it));
			}
			return res;
		}
		throw MessageParseException();
	}

	template<typename T>
	std::vector<T> ParseArrayObj(const JsonValue & json, const char* key)
	{
		return ParseArray<T>(Decent::Net::CommonJsonMsg::GetMember(json, key));
	}
}

template<>
JsonValue & Point2D<int>::ToJson(JsonDoc & doc) const
{
	Tools::JsonSetVal(doc, Point2D::sk_labelX, m_x);
	Tools::JsonSetVal(doc, Point2D::sk_labelY, m_y);
	return doc;
}

template<>
JsonValue & Point2D<double>::ToJson(JsonDoc & doc) const
{
	Tools::JsonSetVal(doc, Point2D::sk_labelX, m_x);
	Tools::JsonSetVal(doc, Point2D::sk_labelY, m_y);
	return doc;
}

constexpr char const GetQuote::sk_labelOrigin[];

JsonValue & GetQuote::ToJson(JsonDoc & doc) const
{
	JsonValue oriRoot = std::move(m_ori.ToJson(doc));
	JsonValue destRoot = std::move(m_dest.ToJson(doc));

	Tools::JsonSetVal(doc, GetQuote::sk_labelOrigin, oriRoot);
	Tools::JsonSetVal(doc, GetQuote::sk_labelDest, destRoot);

	return doc;
}

constexpr char const Path::sk_labelPath[];

std::vector<Point2D<double> > Path::ParsePath(const JsonValue & json)
{
	return ParseArrayObj<Point2D<double> >(json, Path::sk_labelPath);
}

JsonValue& Path::ToJson(JsonDoc& doc) const
{
	std::vector<JsonValue> pathArr;
	pathArr.reserve(m_path.size());

	for (const Point2D<double>& point : m_path)
	{
		pathArr.push_back(std::move(point.ToJson(doc)));
	}

	JsonValue path = std::move(Tools::JsonConstructArray(doc, pathArr));

	Tools::JsonSetVal(doc, Path::sk_labelPath, path);

	return doc;
}

constexpr char const Price::sk_labelPrice[];
constexpr char const Price::sk_labelOpPayment[];

JsonValue& Price::ToJson(JsonDoc& doc) const
{
	Tools::JsonSetVal(doc, Price::sk_labelPrice, m_price);
	Tools::JsonSetVal(doc, Price::sk_labelOpPayment, m_opPayment);
	return doc;
}

constexpr char const Quote::sk_labelGetQuote[];
constexpr char const Quote::sk_labelPath[];
constexpr char const Quote::sk_labelPrice[];
constexpr char const Quote::sk_labelOpPayment[];
constexpr char const Quote::sk_labelPasId[];

JsonValue& Quote::ToJson(JsonDoc& doc) const
{
	JsonValue getQuote = std::move(m_getQuote.ToJson(doc));
	JsonValue path = std::move(m_path.ToJson(doc));
	JsonValue price = std::move(m_price.ToJson(doc));

	Tools::JsonSetVal(doc, sk_labelGetQuote, getQuote);
	Tools::JsonSetVal(doc, sk_labelPath, path);
	Tools::JsonSetVal(doc, sk_labelPrice, price);
	Tools::JsonSetVal(doc, sk_labelOpPayment, m_opPayment);
	Tools::JsonSetVal(doc, sk_labelPasId, m_pasId);

	return doc;
}

constexpr char const SignedQuote::sk_labelQuote[];
constexpr char const SignedQuote::sk_labelSignature[];
constexpr char const SignedQuote::sk_labelCert[];

SignedQuote SignedQuote::SignQuote(const Quote& quote, Decent::Ra::States& state)
{
	using namespace Decent::MbedTlsObj;

	const std::shared_ptr<const Decent::MbedTlsObj::ECKeyPair> prvKeyPtr = state.GetKeyContainer().GetSignKeyPair();
	const Decent::MbedTlsObj::ECKeyPair& prvKey = *prvKeyPtr;
	const std::shared_ptr<const Decent::MbedTlsObj::X509Cert> certPtr = state.GetCertContainer().GetCert();
	const Decent::MbedTlsObj::X509Cert& cert = *certPtr;

	std::string quoteStr = quote.ToString();
	General256Hash hash;
	general_secp256r1_signature_t sign;
	Hasher::Calc<HashType::SHA256>(quoteStr, hash);
	if (!prvKey ||
		!prvKey.EcdsaSign(sign, hash, mbedtls_md_info_from_type(mbedtls_md_type_t::MBEDTLS_MD_SHA256)))
	{
		throw MessageException("Failed to sign the quote!");
	}

	std::string signStr = Tools::SerializeStruct(sign);
	std::string certPemCopy = cert.ToPemString();

	return SignedQuote(std::move(quoteStr), std::move(signStr), std::move(certPemCopy));
}

SignedQuote SignedQuote::ParseSignedQuote(const JsonValue& json, Decent::Ra::States& state, const std::string& appName)
{
	using namespace Decent::MbedTlsObj;

	if (!json.JSON_HAS_MEMBER(SignedQuote::sk_labelQuote) ||
		!json.JSON_HAS_MEMBER(SignedQuote::sk_labelSignature) ||
		!json.JSON_HAS_MEMBER(SignedQuote::sk_labelCert))
	{
		throw MessageParseException();
	}

	std::string quoteStr = ParseValue<std::string>(json[SignedQuote::sk_labelQuote]);
	std::string signStr = ParseValue<std::string>(json[SignedQuote::sk_labelSignature]);
	std::string certPem = ParseValue<std::string>(json[SignedQuote::sk_labelCert]);

	AppX509 cert(certPem);
	TlsConfigWithName tlsCfg(state, TlsConfigWithName::Mode::ServerVerifyPeer, appName);

	uint32_t flag;
	if (!cert ||
		mbedtls_x509_crt_verify_with_profile(cert.Get(), nullptr, nullptr,
		&mbedtls_x509_crt_profile_suiteb, nullptr, &flag, &TlsConfigWithName::CertVerifyCallBack, &tlsCfg) != MBEDTLS_SUCCESS_RET ||
		flag != MBEDTLS_SUCCESS_RET)
	{
		throw MessageParseException();
	}

	general_secp256r1_signature_t sign;
	Tools::DeserializeStruct(sign, signStr);
	General256Hash hash;

	Hasher::Calc<HashType::SHA256>(quoteStr, hash);
	if (!cert.GetEcPublicKey() ||
		!cert.GetEcPublicKey().VerifySign(sign, hash.data(), hash.size()))
	{
		throw MessageParseException();
	}

	return SignedQuote(std::move(quoteStr), std::move(signStr), std::move(certPem));
}

JsonValue& SignedQuote::ToJson(JsonDoc& doc) const
{
	Tools::JsonSetVal(doc, SignedQuote::sk_labelQuote, m_quote);
	Tools::JsonSetVal(doc, SignedQuote::sk_labelSignature, m_sign);
	Tools::JsonSetVal(doc, SignedQuote::sk_labelCert, m_cert);

	return doc;
}

constexpr char const PasContact::sk_labelName[];
constexpr char const PasContact::sk_labelPhone[];

JsonValue & PasContact::ToJson(JsonDoc & doc) const
{
	Tools::JsonSetVal(doc, sk_labelName, m_name);
	Tools::JsonSetVal(doc, sk_labelPhone, m_phone);

	return doc;
}

Decent::General256Hash PasContact::CalcHash() const
{
	using namespace Decent::MbedTlsObj;
	Decent::General256Hash hash;

	Hasher::ArrayBatchedCalc<HashType::SHA256>(hash, sk_labelName, m_name, sk_labelPhone, m_phone);

	return std::move(hash);
}

constexpr char const DriContact::sk_labelName[];
constexpr char const DriContact::sk_labelPhone[];
constexpr char const DriContact::sk_labelLicPlate[];

JsonValue & DriContact::ToJson(JsonDoc & doc) const
{
	Tools::JsonSetVal(doc, sk_labelName, m_name);
	Tools::JsonSetVal(doc, sk_labelPhone, m_phone);
	Tools::JsonSetVal(doc, sk_labelLicPlate, m_licPlate);

	return doc;
}

Decent::General256Hash DriContact::CalcHash() const
{
	using namespace Decent::MbedTlsObj;
	Decent::General256Hash hash;

	Hasher::ArrayBatchedCalc<HashType::SHA256>(hash, sk_labelName, m_name, sk_labelPhone, m_phone, sk_labelLicPlate, m_licPlate);

	return std::move(hash);
}

constexpr char const ConfirmQuote::sk_labelPasContact[];
constexpr char const ConfirmQuote::sk_labelSignedQuote[];

JsonValue & ConfirmQuote::ToJson(JsonDoc & doc) const
{
	JsonValue contact = std::move(m_contact.ToJson(doc));

	Tools::JsonSetVal(doc, sk_labelPasContact, contact);
	Tools::JsonSetVal(doc, sk_labelSignedQuote, m_signQuote);

	return doc;
}

constexpr char const DriSelection::sk_labelDriContact[];
constexpr char const DriSelection::sk_labelTripId[];

JsonValue & DriSelection::ToJson(JsonDoc & doc) const
{
	JsonValue contact = std::move(m_contact.ToJson(doc));

	Tools::JsonSetVal(doc, sk_labelDriContact, contact);
	Tools::JsonSetVal(doc, sk_labelTripId, m_tripId);

	return doc;
}

constexpr char const PasMatchedResult::sk_labelTripId[];
constexpr char const PasMatchedResult::sk_labelDriContact[];

JsonValue & PasMatchedResult::ToJson(JsonDoc & doc) const
{
	JsonValue dirContact = std::move(m_driContact.ToJson(doc));

	Tools::JsonSetVal(doc, sk_labelTripId, m_tripId);
	Tools::JsonSetVal(doc, sk_labelDriContact, dirContact);

	return doc;
}

constexpr char const PasQueryLog::sk_labelUserId[];
constexpr char const PasQueryLog::sk_labelGetQuote[];

JsonValue & PasQueryLog::ToJson(JsonDoc & doc) const
{
	JsonValue getQuote = std::move(m_getQuote.ToJson(doc));

	Tools::JsonSetVal(doc, PasQueryLog::sk_labelUserId, m_userId);
	Tools::JsonSetVal(doc, PasQueryLog::sk_labelGetQuote, getQuote);

	return doc;
}

constexpr char const DriverLoc::sk_labelOrigin[];

JsonValue & DriverLoc::ToJson(JsonDoc & doc) const
{
	JsonValue loc = std::move(m_loc.ToJson(doc));

	Tools::JsonSetVal(doc, DriverLoc::sk_labelOrigin, loc);

	return doc;
}

constexpr char const MatchItem::sk_labelTripId[];
constexpr char const MatchItem::sk_labelPath[];

JsonValue & MatchItem::ToJson(JsonDoc & doc) const
{
	JsonValue path = std::move(m_path.ToJson(doc));

	Tools::JsonSetVal(doc, MatchItem::sk_labelTripId, m_tripId);
	Tools::JsonSetVal(doc, MatchItem::sk_labelPath, path);

	return doc;
}

constexpr char const BestMatches::sk_labelMatches[];

std::vector<MatchItem> BestMatches::ParseMatches(const JsonValue & json)
{
	return ParseArrayObj<MatchItem>(json, sk_labelMatches);
}

JsonValue & BestMatches::ToJson(JsonDoc & doc) const
{
	std::vector<JsonValue> itemArr;
	itemArr.reserve(m_matches.size());

	for (const MatchItem& item : m_matches)
	{
		itemArr.push_back(std::move(item.ToJson(doc)));
	}

	JsonValue matches = std::move(Tools::JsonConstructArray(doc, itemArr));

	Tools::JsonSetVal(doc, sk_labelMatches, matches);

	return doc;
}

constexpr char const DriQueryLog::sk_labelDriverId[];
constexpr char const DriQueryLog::sk_labelLoc[];

JsonValue & DriQueryLog::ToJson(JsonDoc & doc) const
{
	JsonValue loc = std::move(m_loc.ToJson(doc));

	Tools::JsonSetVal(doc, sk_labelDriverId, m_driverId);
	Tools::JsonSetVal(doc, sk_labelLoc, loc);

	return doc;
}

constexpr char const FinalBill::sk_labelQuote[];
constexpr char const FinalBill::sk_labelOpPayment[];
constexpr char const FinalBill::sk_labelDriId[];

JsonValue & FinalBill::ToJson(JsonDoc & doc) const
{
	JsonValue quote = std::move(m_quote.ToJson(doc));

	Tools::JsonSetVal(doc, sk_labelQuote, quote);
	Tools::JsonSetVal(doc, sk_labelOpPayment, m_opPay);
	Tools::JsonSetVal(doc, sk_labelDriId, m_driId);

	return doc;
}

constexpr char const PasReg::sk_labelContact[];
constexpr char const PasReg::sk_labelPayment[];
constexpr char const PasReg::sk_labelCsr[];

JsonValue & PasReg::ToJson(JsonDoc & doc) const
{
	JsonValue contact = std::move(m_contact.ToJson(doc));

	Tools::JsonSetVal(doc, sk_labelContact, contact);
	Tools::JsonSetVal(doc, sk_labelPayment, m_pay);
	Tools::JsonSetVal(doc, sk_labelCsr, m_csr);

	return doc;
}

constexpr char const DriReg::sk_labelContact[];
constexpr char const DriReg::sk_labelPayment[];
constexpr char const DriReg::sk_labelCsr[];
constexpr char const DriReg::sk_labelDriLic[];

JsonValue & DriReg::ToJson(JsonDoc & doc) const
{
	JsonValue contact = std::move(m_contact.ToJson(doc));

	Tools::JsonSetVal(doc, sk_labelContact, contact);
	Tools::JsonSetVal(doc, sk_labelPayment, m_pay);
	Tools::JsonSetVal(doc, sk_labelCsr, m_csr);
	Tools::JsonSetVal(doc, sk_labelDriLic, m_driLic);

	return doc;
}

constexpr char const RequestedPayment::sk_labelPayment[];
constexpr char const RequestedPayment::sk_labelOpPaymnet[];

JsonValue & RequestedPayment::ToJson(JsonDoc & doc) const
{
	Tools::JsonSetVal(doc, sk_labelPayment, m_pay);
	Tools::JsonSetVal(doc, sk_labelOpPaymnet, m_opPay);

	return doc;
}
