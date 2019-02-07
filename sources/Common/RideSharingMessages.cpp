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
#include <DecentApi/Common/MbedTls/MbedTlsHelpers.h>
#include <DecentApi/Common/Ra/Crypto.h>
#include <DecentApi/Common/Ra/TlsConfig.h>
#include <DecentApi/Common/Ra/States.h>
#include <DecentApi/Common/Ra/KeyContainer.h>
#include <DecentApi/Common/Ra/CertContainer.h>

#include "MessageException.h"

using namespace ComMsg;
using namespace Decent;
using namespace Decent::Ra;

namespace
{
	static constexpr int MBEDTLS_SUCCESS_RET = 0;

	template<typename T>
	static inline T ParseSubObj(const JsonValue& json, const char* label)
	{
		if (json.JSON_HAS_MEMBER(label))
		{
			return T(json[label]);
		}
		throw MessageParseException();
	}

}

int Internal::ParseInt(const JsonValue & json)
{
	if (json.JSON_IS_INT())
	{
		return json.JSON_AS_INT32();
	}
	throw MessageParseException();
}

double Internal::ParseDouble(const JsonValue & json)
{
	if (json.JSON_IS_DOUBLE())
	{
		return json.JSON_AS_DOUBLE();
	}
	throw MessageParseException();
}

std::string Internal::ParseString(const JsonValue & json)
{
	if (json.JSON_IS_STRING())
	{
		return json.JSON_AS_STRING();
	}
	throw MessageParseException();
}

std::string Internal::ParseOpPayment(const JsonValue& json, const char* label)
{
	if (json.JSON_HAS_MEMBER(label))
	{
		return ParseString(json[label]);
	}
	throw MessageParseException();
}

std::string JsonMsg::ToString() const
{
	JsonDoc doc;
	ToJson(doc);
	return Tools::Json2StyledString(doc);
}

std::string JsonMsg::ToStyledString() const
{
	JsonDoc doc;
	ToJson(doc);
	return Tools::Json2StyledString(doc);
}

#define ParseAxisFunc(LABEL, TYPECHECKER, VALUEGETTER) if (json.JSON_HAS_MEMBER(LABEL)) \
														{ \
															const JsonValue& jsonAxis = json[LABEL]; \
															if (jsonAxis.TYPECHECKER()) \
															{ \
																return jsonAxis.VALUEGETTER(); \
															} \
														} \
														throw MessageParseException();

template<>
int Point2D<int>::ParseX(const JsonValue & json)
{
	ParseAxisFunc(Point2D::sk_labelX, JSON_IS_INT, JSON_AS_INT32)
}

template<>
double Point2D<double>::ParseX(const JsonValue & json)
{
	ParseAxisFunc(Point2D::sk_labelX, JSON_IS_DOUBLE, JSON_AS_DOUBLE)
}

template<>
int Point2D<int>::ParseY(const JsonValue & json)
{
	ParseAxisFunc(Point2D::sk_labelY, JSON_IS_INT, JSON_AS_INT32)
}

template<>
double Point2D<double>::ParseY(const JsonValue & json)
{
	ParseAxisFunc(Point2D::sk_labelY, JSON_IS_DOUBLE, JSON_AS_DOUBLE)
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

Point2D<double> GetQuote::ParseOri(const JsonValue & json)
{
	if (json.JSON_HAS_MEMBER(GetQuote::sk_labelOrigin))
	{
		return Point2D<double>(json[GetQuote::sk_labelOrigin]);
	}
	throw MessageParseException();
}

Point2D<double> GetQuote::ParseDest(const JsonValue & json)
{
	if (json.JSON_HAS_MEMBER(GetQuote::sk_labelDest))
	{
		return Point2D<double>(json[GetQuote::sk_labelDest]);
	}
	throw MessageParseException();
}

JsonValue & GetQuote::ToJson(JsonDoc & doc) const
{
	JsonValue oriRoot = std::move(m_ori.ToJson(doc));
	JsonValue destRoot = std::move(m_dest.ToJson(doc));

	Tools::JsonSetVal(doc, GetQuote::sk_labelOrigin, oriRoot);
	Tools::JsonSetVal(doc, GetQuote::sk_labelDest, destRoot);

	return doc;
}

std::vector<Point2D<double> > Path::ParsePath(const JsonValue & json)
{
	std::vector<Point2D<double> > res;
	if (json.JSON_HAS_MEMBER(Path::sk_labelPath))
	{
		const JsonValue& jsonPath = json[Path::sk_labelPath];
		if (jsonPath.JSON_IS_ARRAY())
		{
			for (auto it = jsonPath.JSON_ARR_BEGIN(); it != jsonPath.JSON_ARR_END(); ++it)
			{
				res.push_back(JSON_ARR_GETVALUE(it));
			}
			return res;
		}
	}
	throw MessageParseException();
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

double Price::ParsePrice(const JsonValue & json)
{
	if (json.JSON_HAS_MEMBER(Price::sk_labelPrice))
	{
		return Internal::ParseDouble(json[Price::sk_labelPrice]);
	}
	throw MessageParseException();
}

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

GetQuote Quote::ParseQuote(const JsonValue& json)
{
	return ParseSubObj<GetQuote>(json, Quote::sk_labelGetQuote);
}

Path Quote::ParsePath(const JsonValue& json)
{
	return ParseSubObj<Path>(json, Quote::sk_labelPath);
}

Price Quote::ParsePrice(const JsonValue& json)
{
	return ParseSubObj<Price>(json, Quote::sk_labelPrice);
}

JsonValue& Quote::ToJson(JsonDoc& doc) const
{
	JsonValue getQuote = std::move(m_getQuote.ToJson(doc));
	JsonValue path = std::move(m_path.ToJson(doc));
	JsonValue price = std::move(m_price.ToJson(doc));

	Tools::JsonSetVal(doc, Quote::sk_labelGetQuote, getQuote);
	Tools::JsonSetVal(doc, Quote::sk_labelPath, path);
	Tools::JsonSetVal(doc, Quote::sk_labelPrice, price);
	Tools::JsonSetVal(doc, Quote::sk_labelOpPayment, m_opPayment);

	return doc;
}

constexpr char const SignedQuote::sk_labelQuote[];
constexpr char const SignedQuote::sk_labelSignature[];
constexpr char const SignedQuote::sk_labelCert[];

SignedQuote SignedQuote::SignQuote(const Quote& quote, Decent::Ra::States& state)
{
	const std::shared_ptr<const Decent::MbedTlsObj::ECKeyPair> prvKeyPtr = state.GetKeyContainer().GetSignKeyPair();
	const Decent::MbedTlsObj::ECKeyPair& prvKey = *prvKeyPtr;
	const std::shared_ptr<const Decent::MbedTlsObj::X509Cert> certPtr = state.GetCertContainer().GetCert();
	const Decent::MbedTlsObj::X509Cert& cert = *certPtr;

	std::string quoteStr = quote.ToString();
	General256Hash hash;
	general_secp256r1_signature_t sign;
	if (!cert ||
		!MbedTlsHelper::CalcHashSha256(quoteStr, hash) ||
		!prvKey ||
		!prvKey.EcdsaSign(sign, hash, mbedtls_md_info_from_type(mbedtls_md_type_t::MBEDTLS_MD_SHA256)))
	{
		throw MessageException();
	}

	std::string signStr = Tools::SerializeStruct(sign);
	std::string certPemCopy = cert.ToPemString();

	return SignedQuote(std::move(quoteStr), std::move(signStr), std::move(certPemCopy));
}

SignedQuote SignedQuote::ParseSignedQuote(const JsonValue& json, Decent::Ra::States& state, const std::string& appName)
{
	if (!json.JSON_HAS_MEMBER(SignedQuote::sk_labelQuote) ||
		!json.JSON_HAS_MEMBER(SignedQuote::sk_labelSignature) ||
		!json.JSON_HAS_MEMBER(SignedQuote::sk_labelCert))
	{
		throw MessageParseException();
	}

	std::string quoteStr = Internal::ParseString(json[SignedQuote::sk_labelQuote]);
	std::string signStr = Internal::ParseString(json[SignedQuote::sk_labelSignature]);
	std::string certPem = Internal::ParseString(json[SignedQuote::sk_labelCert]);

	AppX509 cert(certPem);
	TlsConfig tlsCfg(appName, state, true);

	uint32_t flag;
	if (!cert ||
		mbedtls_x509_crt_verify_with_profile(cert.Get(), nullptr, nullptr,
		&mbedtls_x509_crt_profile_suiteb, nullptr, &flag, &TlsConfig::CertVerifyCallBack, &tlsCfg) != MBEDTLS_SUCCESS_RET ||
		flag != MBEDTLS_SUCCESS_RET)
	{
		throw MessageParseException();
	}

	general_secp256r1_signature_t sign;
	Tools::DeserializeStruct(sign, signStr);
	General256Hash hash;

	if (!MbedTlsHelper::CalcHashSha256(quoteStr, hash) ||
		!cert.GetEcPublicKey() ||
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

struct TestStruct
{
	TestStruct()
	{
		using TestMsgType = Quote;

		LOGI("Json Msg Test:");

		TestMsgType testMsg(GetQuote(Point2D<double>(1.1, 1.2), Point2D<double>(1.3, 1.4)), 
			Path({ Point2D<double>(1.1, 1.2), Point2D<double>(1.3, 1.4) }), 
			Price(100.12, "TestPayment"), 
			"TestPayment");

		LOGI("1: \n%s", testMsg.ToString().c_str());

		JsonDoc doc;
		Tools::ParseStr2Json(doc, testMsg.ToString());

		TestMsgType testMsg2(doc);

		LOGI("2: \n%s", testMsg2.ToString().c_str());
	}
};

static TestStruct test;
