#pragma once

#include <string>
#include <vector>
#include <DecentApi/Common/Tools/JsonTools.h>

namespace ComMsg
{
	class JsonMsg
	{
	public:
		virtual JSON_EDITION::Value& ToJson(JSON_EDITION::JSON_DOCUMENT_TYPE& doc) const = 0;
		virtual std::string ToString() const;
	};

	template<typename T>
	class Point2D : virtual public JsonMsg
	{
	public:
		static constexpr char const sk_labelX[] = "X";
		static constexpr char const sk_labelY[] = "Y";

		static T ParseX(const JSON_EDITION::Value& json);
		static T ParseY(const JSON_EDITION::Value& json);

	public:
		Point2D() = delete;
		Point2D(const T x, const T y) :
			m_x(x),
			m_y(y)
		{}

		Point2D(const JSON_EDITION::Value& json) :
			m_x(ParseX(json)),
			m_y(ParseY(json))
		{}

		~Point2D() {}

		virtual JSON_EDITION::Value& ToJson(JSON_EDITION::JSON_DOCUMENT_TYPE& doc) const override;

		const T m_x;
		const T m_y;
	};

	template<class T> T Point2D<T>::sk_labelX;
	template<class T> T Point2D<T>::sk_labelY;

	class GetQuote : virtual public JsonMsg
	{
	public:
		static constexpr char const sk_labelOrigin[] = "Ori";
		static constexpr char const sk_labelDest[] = "Dest";

		static Point2D<double> ParseOri(const JSON_EDITION::Value& json);
		static Point2D<double> ParseDest(const JSON_EDITION::Value& json);

	public:
		GetQuote() = delete;
		GetQuote(const Point2D<double>& ori, const Point2D<double>& dest) :
			m_ori(ori),
			m_dest(dest)
		{}

		GetQuote(const JSON_EDITION::Value& json) :
			m_ori(ParseOri(json)),
			m_dest(ParseDest(json))
		{}

		~GetQuote() {}

		virtual JSON_EDITION::Value& ToJson(JSON_EDITION::JSON_DOCUMENT_TYPE& doc) const override;

		const Point2D<double> m_ori;
		const Point2D<double> m_dest;
	};

	class Path :virtual public JsonMsg
	{
	public:
		static constexpr char const sk_labelPath[] = "Path";

		static std::vector<Point2D<double> > ParsePath(const JSON_EDITION::Value& json);

	public:
		Path() = delete;
		Path(const std::vector<Point2D<double> >& path) :
			m_path(path)
		{}

		Path(const JSON_EDITION::Value& json) :
			m_path(ParsePath(json))
		{}

		~Path() {}

		virtual JSON_EDITION::Value& ToJson(JSON_EDITION::JSON_DOCUMENT_TYPE& doc) const override;

		const std::vector<Point2D<double> > m_path;
	};
}
