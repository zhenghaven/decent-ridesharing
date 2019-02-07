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
		class States;
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
		template<typename T>
		T ParseValue(const JsonValue & json);
		template<>
		int Internal::ParseValue<int>(const JsonValue & json);
		template<>
		double Internal::ParseValue<double>(const JsonValue & json);
		template<>
		std::string Internal::ParseValue<std::string>(const JsonValue & json);

		template<typename T>
		T ParseObj(const JsonValue & json, const char* label);
		template<>
		int Internal::ParseObj<int>(const JsonValue & json, const char * label);
		template<>
		double Internal::ParseObj<double>(const JsonValue & json, const char * label);
		template<>
		std::string Internal::ParseObj<std::string>(const JsonValue & json, const char * label);

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

		static Point2D<T> Parse(const JsonValue& json, const char* label);

	public:
		Point2D() = delete;
		Point2D(const T& x, const T& y) :
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
			m_x(Internal::ParseObj<T>(json, sk_labelX)),
			m_y(Internal::ParseObj<T>(json, sk_labelY))
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

		static GetQuote Parse(const JsonValue& json, const char* label);

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
			GetQuote(Point2D<double>::Parse(json, sk_labelOrigin), 
				Point2D<double>::Parse(json, sk_labelDest))
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

		static Path Parse(const JsonValue& json, const char* label);

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

		static Price Parse(const JsonValue& json, const char* label);

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
			Price(Internal::ParseObj<double>(json, sk_labelPrice),
				Internal::ParseOpPayment(json, sk_labelOpPayment))
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

		static Quote Parse(const JsonValue& json, const char* label);

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
			Quote(GetQuote::Parse(json, sk_labelGetQuote),
				Path::Parse(json, sk_labelPath),
				Price::Parse(json, sk_labelPrice),
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

		static SignedQuote SignQuote(const Quote& quote, Decent::Ra::States& state);
		static SignedQuote ParseSignedQuote(const JsonValue& json, Decent::Ra::States& state, const std::string& appName);

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

	class ConfirmQuote : virtual public JsonMsg
	{
	public:
		static constexpr char const sk_labelName[] = "Name";
		static constexpr char const sk_labelPhone[] = "Phone";
		static constexpr char const sk_labelSignedQuote[] = "SgQuote";

	public:
		ConfirmQuote() = delete;

		ConfirmQuote(const std::string& name, const std::string& phone, const std::string& signQuote) :
			m_name(name),
			m_phone(phone),
			m_signQuote(signQuote)
		{}

		ConfirmQuote(std::string&& name, std::string&& phone, std::string&& signQuote) :
			m_name(std::forward<std::string>(name)),
			m_phone(std::forward<std::string>(phone)),
			m_signQuote(std::forward<std::string>(signQuote))
		{}

		ConfirmQuote(const ConfirmQuote& rhs) :
			ConfirmQuote(rhs.m_name, rhs.m_phone, rhs.m_signQuote)
		{}

		ConfirmQuote(ConfirmQuote&& rhs) :
			ConfirmQuote(std::forward<std::string>(rhs.m_name),
				std::forward<std::string>(rhs.m_phone),
				std::forward<std::string>(rhs.m_signQuote))
		{}

		ConfirmQuote(const JsonValue& json) :
			ConfirmQuote(Internal::ParseObj<std::string>(json, sk_labelName),
				Internal::ParseObj<std::string>(json, sk_labelPhone),
				Internal::ParseObj<std::string>(json, sk_labelSignedQuote))
		{}

		~ConfirmQuote() {}

		virtual JsonValue& ToJson(JsonDoc& doc) const override;

		const std::string& GetName() const { return m_name; }
		const std::string& GetPhone() const { return m_phone; }
		const std::string& GetSignQuote() const { return m_signQuote; }

	private:
		std::string m_name;
		std::string m_phone;
		std::string m_signQuote;
	};

	class ConfirmedQuote : virtual public JsonMsg
	{
	public:
		static constexpr char const sk_labelName[] = "Name";
		static constexpr char const sk_labelPhone[] = "Phone";
		static constexpr char const sk_labelQuote[] = "Quote";
		static constexpr char const sk_labelOpPayment[] = "OpPayment";

	public:
		ConfirmedQuote() = delete;

		ConfirmedQuote(const std::string& name, const std::string& phone, const Quote& quote, const std::string& opPayment) :
			m_name(name),
			m_phone(phone),
			m_quote(quote),
			m_opPayment(opPayment)
		{}

		ConfirmedQuote(std::string&& name, std::string&& phone, Quote&& quote, std::string&& opPayment) :
			m_name(std::forward<std::string>(name)),
			m_phone(std::forward<std::string>(phone)),
			m_quote(std::forward<Quote>(quote)),
			m_opPayment(std::forward<std::string>(opPayment))
		{}

		ConfirmedQuote(const ConfirmedQuote& rhs) :
			ConfirmedQuote(rhs.m_name, rhs.m_phone, rhs.m_quote, rhs.m_opPayment)
		{}

		ConfirmedQuote(ConfirmedQuote&& rhs) :
			ConfirmedQuote(std::forward<std::string>(rhs.m_name),
				std::forward<std::string>(rhs.m_phone),
				std::forward<Quote>(rhs.m_quote),
				std::forward<std::string>(rhs.m_opPayment))
		{}

		ConfirmedQuote(const JsonValue& json) :
			ConfirmedQuote(Internal::ParseObj<std::string>(json, sk_labelName),
				Internal::ParseObj<std::string>(json, sk_labelPhone),
				Quote::Parse(json, sk_labelQuote),
				Internal::ParseObj<std::string>(json, sk_labelOpPayment))
		{}

		~ConfirmedQuote() {}

		virtual JsonValue& ToJson(JsonDoc& doc) const override;

		const std::string& GetName() const { return m_name; }
		const std::string& GetPhone() const { return m_phone; }
		const Quote& GetSignQuote() const { return m_quote; }
		const std::string& GetOpPayment() const { return m_opPayment; }

	private:
		std::string m_name;
		std::string m_phone;
		Quote m_quote;
		std::string m_opPayment;
	};

	class TripId : virtual public JsonMsg
	{
	public:
		static constexpr char const sk_labelId[] = "Id";

		static TripId Parse(const JsonValue& json, const char* label);

	public:
		TripId() = delete;

		TripId(const std::string& id) :
			m_id(id)
		{}

		TripId(std::string&& id) :
			m_id(std::forward<std::string>(id))
		{}

		TripId(const TripId& rhs) :
			TripId(rhs.m_id)
		{}

		TripId(TripId&& rhs) :
			TripId(std::forward<std::string>(rhs.m_id))
		{}

		TripId(const JsonValue& json) :
			TripId(Internal::ParseObj<std::string>(json, sk_labelId))
		{}

		~TripId() {}

		virtual JsonValue& ToJson(JsonDoc& doc) const override;

		const std::string& GetId() const { return m_id; }

	private:
		std::string m_id;
	};

	class TripMatcherAddr : virtual public JsonMsg
	{
	public:
		static constexpr char const sk_labelIp[] = "IP";
		static constexpr char const sk_labelPort[] = "Port";
		static constexpr char const sk_labelTripId[] = "TripId";

	public:
		TripMatcherAddr() = delete;

		TripMatcherAddr(const uint32_t Ip, const uint32_t port, const TripId& tripId) :
			m_ip(Ip),
			m_port(port),
			m_tripId(tripId)
		{}

		TripMatcherAddr(const uint32_t Ip, const uint32_t port, TripId&& tripId) :
			m_ip(Ip),
			m_port(port),
			m_tripId(std::forward<TripId>(tripId))
		{}

		TripMatcherAddr(const TripMatcherAddr& rhs) :
			TripMatcherAddr(rhs.m_ip, rhs.m_port, rhs.m_tripId)
		{}

		TripMatcherAddr(TripMatcherAddr&& rhs) :
			TripMatcherAddr(rhs.m_ip, rhs.m_port, std::forward<TripId>(rhs.m_tripId))
		{}

		TripMatcherAddr(const JsonValue& json) :
			TripMatcherAddr(static_cast<uint32_t>(Internal::ParseObj<int>(json, sk_labelIp)),
				static_cast<uint16_t>(Internal::ParseObj<int>(json, sk_labelPort)),
				TripId::Parse(json, sk_labelTripId))
		{}

		~TripMatcherAddr() {}

		virtual JsonValue& ToJson(JsonDoc& doc) const override;

		uint32_t GetIp() const { return m_ip; }
		uint16_t GetPort() const { return m_port; }
		const TripId& GetTripId() const { return m_tripId; }

	private:
		uint32_t m_ip;
		uint16_t m_port;
		TripId m_tripId;
	};

	class PasQueryLog : virtual public JsonMsg
	{
	public:
		static constexpr char const sk_labelUserId[] = "UserId";
		static constexpr char const sk_labelGetQuote[] = "GetQuote";

	public:
		PasQueryLog() = delete;

		PasQueryLog(const std::string& userId, const GetQuote& getQuote) :
			m_userId(userId),
			m_getQuote(getQuote)
		{}

		PasQueryLog(std::string&& userId, GetQuote&& getQuote) :
			m_userId(std::forward<std::string>(userId)),
			m_getQuote(std::forward<GetQuote>(getQuote))
		{}

		PasQueryLog(const PasQueryLog& rhs) :
			PasQueryLog(rhs.m_userId, rhs.m_getQuote)
		{}

		PasQueryLog(PasQueryLog&& rhs) :
			PasQueryLog(std::forward<std::string>(rhs.m_userId),
				std::forward<GetQuote>(rhs.m_getQuote))
		{}

		PasQueryLog(const JsonValue& json) :
			PasQueryLog(Internal::ParseObj<std::string>(json, sk_labelUserId),
				GetQuote::Parse(json, sk_labelGetQuote))
		{}

		~PasQueryLog() {}

		virtual JsonValue& ToJson(JsonDoc& doc) const override;

		const std::string& GetUserId() const { return m_userId; }
		const GetQuote& GetGetQuote() const { return m_getQuote; }

	private:
		std::string m_userId;
		GetQuote m_getQuote;
	};

	class DriverLoc : virtual public JsonMsg
	{
	public:
		static constexpr char const sk_labelOrigin[] = "Loc";

	public:
		DriverLoc() = delete;
		DriverLoc(const Point2D<double>& loc) :
			m_loc(loc)
		{}

		DriverLoc(Point2D<double>&& loc) :
			m_loc(std::forward<Point2D<double> >(loc))
		{}

		DriverLoc(const DriverLoc& rhs) :
			DriverLoc(rhs.m_loc)
		{}

		DriverLoc(DriverLoc&& rhs) :
			DriverLoc(std::forward<Point2D<double> >(rhs.m_loc))
		{}

		DriverLoc(const JsonValue& json) :
			DriverLoc(Point2D<double>::Parse(json, sk_labelOrigin))
		{}

		~DriverLoc() {}

		virtual JsonValue& ToJson(JsonDoc& doc) const override;

		const Point2D<double>& GetLoc() { return m_loc; }

	private:
		Point2D<double> m_loc;
	};
}
