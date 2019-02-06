#pragma once

#include <string>
#include <vector>

#ifdef ENCLAVE_ENVIRONMENT
namespace rapidjson
{
	class CrtAllocator;

	template <typename BaseAllocator>
	class MemoryPoolAllocator;

	template <typename Encoding, typename Allocator>
	class GenericValue;

	template<typename CharType>
	struct UTF8;

	template <typename Encoding, typename Allocator, typename StackAllocator>
	class GenericDocument;

	typedef GenericValue<UTF8<char>, MemoryPoolAllocator<CrtAllocator> > Value;
	typedef GenericDocument<UTF8<char>, MemoryPoolAllocator<CrtAllocator>, CrtAllocator> Document;
}
#else
namespace Json
{
	class Value;
}
#endif // ENCLAVE_ENVIRONMENT

namespace Decent
{
	namespace MbedTlsObj
	{
		class ECKeyPair;
	}
	namespace Ra
	{
		class State;
	}
}

namespace ComMsg
{

#ifdef ENCLAVE_ENVIRONMENT
	typedef rapidjson::Value JsonValue;
	typedef rapidjson::Document JsonDoc;
#else
	typedef Json::Value JsonValue;
	typedef Json::Value JsonDoc;
#endif // ENCLAVE_ENVIRONMENT

	namespace Internal
	{
		int ParseInt(const JsonValue & json);
		double ParseDouble(const JsonValue & json);
		std::string ParseString(const JsonValue & json);
		std::string ParseOpPayment(const JsonValue& json, const char* label);
	}

	class JsonMsg
	{
	public:
		virtual JsonValue& ToJson(JsonDoc& doc) const = 0;
		virtual std::string ToString() const;
		virtual std::string ToStyledString() const;
	};

	template<typename T>
	class Point2D : virtual public JsonMsg
	{
	public:
		static constexpr char const sk_labelX[] = "X";
		static constexpr char const sk_labelY[] = "Y";

		static T ParseX(const JsonValue& json);
		static T ParseY(const JsonValue& json);

	public:
		Point2D() = delete;
		Point2D(const T x, const T y) :
			m_x(x),
			m_y(y)
		{}

		//Point2D(T&& x, T&& y) :
		//	m_x(std::forward<T>(x)),
		//	m_y(std::forward<T>(y))
		//{}

		Point2D(const Point2D& rhs) :
			Point2D(rhs.m_x, rhs.m_y)
		{}

		Point2D(Point2D&& rhs) :
			Point2D(rhs.m_x, rhs.m_y)
		{}

		Point2D(const JsonValue& json) :
			m_x(ParseX(json)),
			m_y(ParseY(json))
		{}

		~Point2D() {}

		virtual JsonValue& ToJson(JsonDoc& doc) const override;

		const T& GetX() const { return m_x; }
		const T& GetY() const { return m_y; }

	private:
		T m_x;
		T m_y;
	};

	template<class T> T Point2D<T>::sk_labelX;
	template<class T> T Point2D<T>::sk_labelY;

	class GetQuote : virtual public JsonMsg
	{
	public:
		static constexpr char const sk_labelOrigin[] = "Ori";
		static constexpr char const sk_labelDest[] = "Dest";

		static Point2D<double> ParseOri(const JsonValue& json);
		static Point2D<double> ParseDest(const JsonValue& json);

	public:
		GetQuote() = delete;
		GetQuote(const Point2D<double>& ori, const Point2D<double>& dest) :
			m_ori(ori),
			m_dest(dest)
		{}

		GetQuote(Point2D<double>&& ori, Point2D<double>&& dest) :
			m_ori(std::forward<Point2D<double> >(ori)),
			m_dest(std::forward<Point2D<double> >(dest))
		{}

		GetQuote(const GetQuote& rhs) :
			GetQuote(rhs.m_ori, rhs.m_dest)
		{}

		GetQuote(GetQuote&& rhs) :
			GetQuote(std::forward<Point2D<double> >(rhs.m_ori), std::forward<Point2D<double> >(rhs.m_dest))
		{}

		GetQuote(const JsonValue& json) :
			GetQuote(ParseOri(json), ParseDest(json))
		{}

		~GetQuote() {}

		virtual JsonValue& ToJson(JsonDoc& doc) const override;

		const Point2D<double>& GetOri() { return m_ori; }
		const Point2D<double>& GetDest() { return m_dest; }

	private:
		Point2D<double> m_ori;
		Point2D<double> m_dest;
	};

	class Path :virtual public JsonMsg
	{
	public:
		static constexpr char const sk_labelPath[] = "Path";

		static std::vector<Point2D<double> > ParsePath(const JsonValue& json);

	public:
		Path() = delete;
		Path(const std::vector<Point2D<double> >& path) :
			m_path(path)
		{}

		Path(std::vector<Point2D<double> >&& path) :
			m_path(std::forward<std::vector<Point2D<double> > >(path))
		{}

		Path(const Path& rhs) :
			Path(rhs.m_path)
		{}

		Path(Path&& rhs) :
			Path(std::forward<std::vector<Point2D<double> > >(rhs.m_path))
		{}

		Path(const JsonValue& json) :
			Path(ParsePath(json))
		{}

		~Path() {}

		virtual JsonValue& ToJson(JsonDoc& doc) const override;

		const std::vector<Point2D<double> >& GetPath() const { return m_path; }

	private:
		std::vector<Point2D<double> > m_path;
	};

	class Price : virtual public JsonMsg
	{
	public:
		static constexpr char const sk_labelPrice[] = "Price";
		static constexpr char const sk_labelOpPayment[] = "OpPay";

		static double ParsePrice(const JsonValue& json);

	public:
		Price() = delete;
		Price(const double price, const std::string& opPayment) :
			m_price(price),
			m_opPayment(opPayment)
		{}

		Price(double price, std::string&& opPayment) :
			m_price(price),
			m_opPayment(std::forward<std::string>(opPayment))
		{}

		Price(const Price& rhs) :
			Price(rhs.m_price, rhs.m_opPayment)
		{}

		Price(Price&& rhs) :
			Price(std::forward<double>(rhs.m_price), std::forward<std::string>(rhs.m_opPayment))
		{}

		Price(const JsonValue& json) :
			Price(ParsePrice(json), Internal::ParseOpPayment(json, sk_labelOpPayment))
		{}

		~Price() {}

		virtual JsonValue& ToJson(JsonDoc& doc) const override;

		double GetPrice() const { return m_price; }
		const std::string& GetOpPayment() const { return m_opPayment; }

	private:
		double m_price;
		std::string m_opPayment;
	};

	class Quote : virtual public JsonMsg
	{
	public:
		static constexpr char const sk_labelGetQuote[] = "GetQuote";
		static constexpr char const sk_labelPath[] = "Path";
		static constexpr char const sk_labelPrice[] = "Price";
		static constexpr char const sk_labelOpPayment[] = "OpPay";

		static GetQuote ParseQuote(const JsonValue& json);
		static Path ParsePath(const JsonValue& json);
		static Price ParsePrice(const JsonValue& json);

	public:
		Quote() = delete;
		Quote(const GetQuote& quote, const Path& path, const Price& price, const std::string& opPayment) :
			m_getQuote(quote),
			m_path(path),
			m_price(price),
			m_opPayment(opPayment)
		{}

		Quote(GetQuote&& quote, Path&& path, Price&& price, std::string&& opPayment) :
			m_getQuote(std::forward<GetQuote>(quote)),
			m_path(std::forward<Path>(path)),
			m_price(std::forward<Price>(price)),
			m_opPayment(std::forward<std::string>(opPayment))
		{}

		Quote(const Quote& rhs) :
			Quote(rhs.m_getQuote, rhs.m_path, rhs.m_price, rhs.m_opPayment)
		{}

		Quote(Quote&& rhs) :
			Quote(std::forward<GetQuote>(rhs.m_getQuote), 
				std::forward<Path>(rhs.m_path), 
				std::forward<Price>(rhs.m_price),
				std::forward<std::string>(rhs.m_opPayment))
		{}

		Quote(const JsonValue& json) :
			Quote(ParseQuote(json),
				ParsePath(json),
				ParsePrice(json),
				Internal::ParseOpPayment(json, sk_labelOpPayment))
		{}

		~Quote() {}

		virtual JsonValue& ToJson(JsonDoc& doc) const override;

		const GetQuote& GetGetQuote() const { return m_getQuote; }
		const Path& GetPath() const { return m_path; }
		const Price& GetPrice() const { return m_price; }
		const std::string& GetOpPayment() const { return m_opPayment; }

	private:
		GetQuote m_getQuote;
		Path m_path;
		Price m_price;
		std::string m_opPayment;
	};

	class SignedQuote : virtual public JsonMsg
	{
	public:
		static constexpr char const sk_labelQuote[] = "Quote";
		static constexpr char const sk_labelSignature[] = "Sign";
		static constexpr char const sk_labelCert[] = "Cert";

		static SignedQuote SignQuote(const Quote& quote, const Decent::MbedTlsObj::ECKeyPair& prvKey, const std::string& certPem);
		static SignedQuote ParseSignedQuote(const JsonValue& json, const Decent::Ra::State& state, const std::string& appName);

	public:
		SignedQuote() = delete;

		SignedQuote(const SignedQuote& rhs) :
			SignedQuote(rhs.m_quote, rhs.m_sign, rhs.m_cert)
		{}

		SignedQuote(SignedQuote&& rhs) :
			SignedQuote(std::forward<std::string>(rhs.m_quote),
				std::forward<std::string>(rhs.m_sign),
				std::forward<std::string>(rhs.m_cert))
		{}

		~SignedQuote() {}

		virtual JsonValue& ToJson(JsonDoc& doc) const override;

		const std::string& GetQuote() const { return m_quote; }

	private:
		SignedQuote(const std::string& quote, const std::string& sign, const std::string& cert) :
			m_quote(quote),
			m_sign(sign),
			m_cert(cert)
		{}

		SignedQuote(std::string&& quote, std::string&& sign, std::string&& cert) :
			m_quote(std::forward<std::string>(quote)),
			m_sign(std::forward<std::string>(sign)),
			m_cert(std::forward<std::string>(cert))
		{}

		std::string m_quote;
		std::string m_sign;
		std::string m_cert;
	};
}
