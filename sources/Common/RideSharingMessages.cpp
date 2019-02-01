#include "RideSharingMessages.h"

#ifdef ENCLAVE_ENVIRONMENT
#include <rapidjson/document.h>
#else
#include <json/json.h>
#endif

#include <DecentApi/Common/Tools/JsonTools.h>
#include <DecentApi/Common/Common.h>

#include "MessageException.h"

using namespace ComMsg;
using namespace Decent;

std::string JsonMsg::ToString() const
{
	JSON_EDITION::JSON_DOCUMENT_TYPE doc;
	ToJson(doc);
	return Tools::Json2StyledString(doc);
}

#define ParseAxisFunc(LABEL, TYPECHECKER, VALUEGETTER) if (json.JSON_HAS_MEMBER(LABEL)) \
														{ \
															const JSON_EDITION::Value& jsonAxis = json[LABEL]; \
															if (jsonAxis.TYPECHECKER()) \
															{ \
																return jsonAxis.VALUEGETTER(); \
															} \
														} \
														throw MessageParseException();

template<>
int Point2D<int>::ParseX(const JSON_EDITION::Value & json)
{
	ParseAxisFunc(Point2D::sk_labelX, JSON_IS_INT, JSON_AS_INT32)
}

template<>
double Point2D<double>::ParseX(const JSON_EDITION::Value & json)
{
	ParseAxisFunc(Point2D::sk_labelX, JSON_IS_DOUBLE, JSON_AS_DOUBLE)
}

template<>
int Point2D<int>::ParseY(const JSON_EDITION::Value & json)
{
	ParseAxisFunc(Point2D::sk_labelY, JSON_IS_INT, JSON_AS_INT32)
}

template<>
double Point2D<double>::ParseY(const JSON_EDITION::Value & json)
{
	ParseAxisFunc(Point2D::sk_labelY, JSON_IS_DOUBLE, JSON_AS_DOUBLE)
}

template<>
JSON_EDITION::Value & Point2D<int>::ToJson(JSON_EDITION::JSON_DOCUMENT_TYPE & doc) const
{
	Tools::JsonSetVal(doc, Point2D::sk_labelX, m_x);
	Tools::JsonSetVal(doc, Point2D::sk_labelY, m_y);
	return doc;
}

template<>
JSON_EDITION::Value & Point2D<double>::ToJson(JSON_EDITION::JSON_DOCUMENT_TYPE & doc) const
{
	Tools::JsonSetVal(doc, Point2D::sk_labelX, m_x);
	Tools::JsonSetVal(doc, Point2D::sk_labelY, m_y);
	return doc;
}

constexpr char const GetQuote::sk_labelOrigin[];

Point2D<double> GetQuote::ParseOri(const JSON_EDITION::Value & json)
{
	if (json.JSON_HAS_MEMBER(GetQuote::sk_labelOrigin))
	{
		return Point2D<double>(json[GetQuote::sk_labelOrigin]);
	}
	throw MessageParseException();
}

Point2D<double> GetQuote::ParseDest(const JSON_EDITION::Value & json)
{
	if (json.JSON_HAS_MEMBER(GetQuote::sk_labelDest))
	{
		return Point2D<double>(json[GetQuote::sk_labelDest]);
	}
	throw MessageParseException();
}

JSON_EDITION::Value & GetQuote::ToJson(JSON_EDITION::JSON_DOCUMENT_TYPE & doc) const
{
	JSON_EDITION::Value oriRoot = std::move(m_ori.ToJson(doc));
	JSON_EDITION::Value destRoot = std::move(m_dest.ToJson(doc));

	Tools::JsonSetVal(doc, GetQuote::sk_labelOrigin, oriRoot);
	Tools::JsonSetVal(doc, GetQuote::sk_labelDest, destRoot);

	return doc;
}

std::vector<Point2D<double> > Path::ParsePath(const JSON_EDITION::Value & json)
{
	std::vector<Point2D<double> > res;
	if (json.JSON_HAS_MEMBER(Path::sk_labelPath))
	{
		const JSON_EDITION::Value& jsonPath = json[Path::sk_labelPath];
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

JSON_EDITION::Value& Path::ToJson(JSON_EDITION::JSON_DOCUMENT_TYPE& doc) const
{
	std::vector<JSON_EDITION::Value> pathArr;
	pathArr.reserve(m_path.size());

	for (const Point2D<double>& point : m_path)
	{
		pathArr.push_back(std::move(point.ToJson(doc)));
	}

	JSON_EDITION::Value path = std::move(Tools::JsonConstructArray(doc, pathArr));

	Tools::JsonSetVal(doc, Path::sk_labelPath, path);

	return doc;
}

struct TestStruct
{
	TestStruct()
	{
		LOGI("Json Msg Test:");

		Path testMsg({ Point2D<double>(1.4, 1.3), Point2D<double>(1.2, 1.1) });

		LOGI("1: \n%s", testMsg.ToString().c_str());

		JSON_EDITION::JSON_DOCUMENT_TYPE doc;
		Tools::ParseStr2Json(doc, testMsg.ToString());

		Path testMsg2(doc);

		LOGI("2: \n%s", testMsg2.ToString().c_str());
	}
};

static TestStruct test;
