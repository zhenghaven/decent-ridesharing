#pragma once

#include <string>
#include <vector>

#include <DecentApi/Common/Net/CommonMessages.h>
#include <DecentApi/Common/GeneralKeyTypes.h>

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

namespace RideShare
{
	namespace ComMsg
	{

		typedef Decent::Tools::JsonValue JsonValue;
		typedef Decent::Tools::JsonDoc   JsonDoc;

		template<typename T>
		class Point2D : virtual public Decent::Net::CommonJsonMsg
		{
		public:
			static constexpr char const sk_labelX[] = "X";
			static constexpr char const sk_labelY[] = "Y";

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
				m_x(ParseValue<T>(json, sk_labelX)),
				m_y(ParseValue<T>(json, sk_labelY))
			{}

			~Point2D() {}

			virtual JsonValue& ToJson(JsonDoc& doc) const override;

			const T& GetX() const { return m_x; }
			const T& GetY() const { return m_y; }

		private:
			T m_x;
			T m_y;
		};

		//template<class T> constexpr char const Point2D<T>::sk_labelX[];
		//template<class T> constexpr char const Point2D<T>::sk_labelY[];

		class GetQuote : virtual public Decent::Net::CommonJsonMsg
		{
		public:
			static constexpr char const sk_labelOrigin[] = "Ori";
			static constexpr char const sk_labelDest[] = "Dest";

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
				GetQuote(ParseSubMessage<Point2D<double> >(json, sk_labelOrigin),
					ParseSubMessage<Point2D<double> >(json, sk_labelDest))
			{}

			~GetQuote() {}

			virtual JsonValue& ToJson(JsonDoc& doc) const override;

			const Point2D<double>& GetOri() const { return m_ori; }
			const Point2D<double>& GetDest() const { return m_dest; }

		private:
			Point2D<double> m_ori;
			Point2D<double> m_dest;
		};

		class Path :virtual public Decent::Net::CommonJsonMsg
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

		class Price : virtual public Decent::Net::CommonJsonMsg
		{
		public:
			static constexpr char const sk_labelPrice[] = "Price";
			static constexpr char const sk_labelOpPayment[] = "OpPay";

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
				Price(ParseValue<double>(json, sk_labelPrice),
					ParseValue<std::string>(json, sk_labelOpPayment))
			{}

			~Price() {}

			virtual JsonValue& ToJson(JsonDoc& doc) const override;

			double GetPrice() const { return m_price; }
			const std::string& GetOpPayment() const { return m_opPayment; }

		private:
			double m_price;
			std::string m_opPayment;
		};

		class Quote : virtual public Decent::Net::CommonJsonMsg
		{
		public:
			static constexpr char const sk_labelGetQuote[] = "GetQuote";
			static constexpr char const sk_labelPath[] = "Path";
			static constexpr char const sk_labelPrice[] = "Price";
			static constexpr char const sk_labelOpPayment[] = "OpPay";
			static constexpr char const sk_labelPasId[] = "PasId";

		public:
			Quote() = delete;
			Quote(const GetQuote& quote, const Path& path, const Price& price, const std::string& opPayment, const std::string& pasId) :
				m_getQuote(quote),
				m_path(path),
				m_price(price),
				m_opPayment(opPayment),
				m_pasId(pasId)
			{}

			Quote(GetQuote&& quote, Path&& path, Price&& price, std::string&& opPayment, std::string&& pasId) :
				m_getQuote(std::forward<GetQuote>(quote)),
				m_path(std::forward<Path>(path)),
				m_price(std::forward<Price>(price)),
				m_opPayment(std::forward<std::string>(opPayment)),
				m_pasId(std::forward<std::string>(pasId))
			{}

			Quote(const Quote& rhs) :
				Quote(rhs.m_getQuote, rhs.m_path, rhs.m_price, rhs.m_opPayment, rhs.m_pasId)
			{}

			Quote(Quote&& rhs) :
				Quote(std::forward<GetQuote>(rhs.m_getQuote),
					std::forward<Path>(rhs.m_path),
					std::forward<Price>(rhs.m_price),
					std::forward<std::string>(rhs.m_opPayment),
					std::forward<std::string>(rhs.m_pasId))
			{}

			Quote(const JsonValue& json) :
				Quote(ParseSubMessage<GetQuote>(json, sk_labelGetQuote),
					ParseSubMessage<Path>(json, sk_labelPath),
					ParseSubMessage<Price>(json, sk_labelPrice),
					ParseValue<std::string>(json, sk_labelOpPayment),
					ParseValue<std::string>(json, sk_labelPasId))
			{}

			~Quote() {}

			virtual JsonValue& ToJson(JsonDoc& doc) const override;

			const GetQuote& GetGetQuote() const { return m_getQuote; }
			const Path& GetPath() const { return m_path; }
			const Price& GetPrice() const { return m_price; }
			const std::string& GetOpPayment() const { return m_opPayment; }
			const std::string& GetPasId() const { return m_pasId; }

		private:
			GetQuote m_getQuote;
			Path m_path;
			Price m_price;
			std::string m_opPayment;
			std::string m_pasId;
		};

		class SignedQuote : virtual public Decent::Net::CommonJsonMsg
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

		class PasContact : virtual public Decent::Net::CommonJsonMsg
		{
		public:
			static constexpr char const sk_labelName[] = "Name";
			static constexpr char const sk_labelPhone[] = "Phone";

		public:
			PasContact() = delete;

			PasContact(const std::string& name, const std::string& phone) :
				m_name(name),
				m_phone(phone)
			{}

			PasContact(std::string&& name, std::string&& phone) :
				m_name(std::forward<std::string>(name)),
				m_phone(std::forward<std::string>(phone))
			{}

			PasContact(const PasContact& rhs) :
				PasContact(rhs.m_name, rhs.m_phone)
			{}

			PasContact(PasContact&& rhs) :
				PasContact(std::forward<std::string>(rhs.m_name),
					std::forward<std::string>(rhs.m_phone))
			{}

			PasContact(const JsonValue& json) :
				PasContact(ParseValue<std::string>(json, sk_labelName),
					ParseValue<std::string>(json, sk_labelPhone))
			{}

			~PasContact() {}

			virtual JsonValue& ToJson(JsonDoc& doc) const override;

			const std::string& GetName() const { return m_name; }
			const std::string& GetPhone() const { return m_phone; }

			virtual Decent::General256Hash CalcHash() const;

		private:
			std::string m_name;
			std::string m_phone;
		};

		class DriContact : virtual public Decent::Net::CommonJsonMsg
		{
		public:
			static constexpr char const sk_labelName[] = "Name";
			static constexpr char const sk_labelPhone[] = "Phone";
			static constexpr char const sk_labelLicPlate[] = "Plate";

		public:
			DriContact() = delete;

			DriContact(const std::string& name, const std::string& phone, const std::string& licPlate) :
				m_name(name),
				m_phone(phone),
				m_licPlate(licPlate)
			{}

			DriContact(std::string&& name, std::string&& phone, std::string&& licPlate) :
				m_name(std::forward<std::string>(name)),
				m_phone(std::forward<std::string>(phone)),
				m_licPlate(std::forward<std::string>(licPlate))
			{}

			DriContact(const DriContact& rhs) :
				DriContact(rhs.m_name, rhs.m_phone, rhs.m_licPlate)
			{}

			DriContact(DriContact&& rhs) :
				DriContact(std::forward<std::string>(rhs.m_name),
					std::forward<std::string>(rhs.m_phone),
					std::forward<std::string>(rhs.m_licPlate))
			{}

			DriContact(const JsonValue& json) :
				DriContact(ParseValue<std::string>(json, sk_labelName),
					ParseValue<std::string>(json, sk_labelPhone),
					ParseValue<std::string>(json, sk_labelLicPlate))
			{}

			~DriContact() {}

			virtual JsonValue& ToJson(JsonDoc& doc) const override;

			const std::string& GetName() const { return m_name; }
			const std::string& GetPhone() const { return m_phone; }

			virtual Decent::General256Hash CalcHash() const;

		private:
			std::string m_name;
			std::string m_phone;
			std::string m_licPlate;
		};

		class ConfirmQuote : virtual public Decent::Net::CommonJsonMsg
		{
		public:
			static constexpr char const sk_labelPasContact[] = "Contact";
			static constexpr char const sk_labelSignedQuote[] = "SgQuote";

		public:
			ConfirmQuote() = delete;

			ConfirmQuote(const PasContact& contact, const std::string& signQuote) :
				m_contact(contact),
				m_signQuote(signQuote)
			{}

			ConfirmQuote(PasContact&& contact, std::string&& signQuote) :
				m_contact(std::forward<PasContact>(contact)),
				m_signQuote(std::forward<std::string>(signQuote))
			{}

			ConfirmQuote(const ConfirmQuote& rhs) :
				ConfirmQuote(rhs.m_contact, rhs.m_signQuote)
			{}

			ConfirmQuote(ConfirmQuote&& rhs) :
				ConfirmQuote(std::forward<PasContact>(rhs.m_contact),
					std::forward<std::string>(rhs.m_signQuote))
			{}

			ConfirmQuote(const JsonValue& json) :
				ConfirmQuote(ParseSubMessage<PasContact>(json, sk_labelPasContact),
					ParseValue<std::string>(json, sk_labelSignedQuote))
			{}

			~ConfirmQuote() {}

			virtual JsonValue& ToJson(JsonDoc& doc) const override;

			const PasContact& GetContact() const { return m_contact; }
			const std::string& GetSignQuote() const { return m_signQuote; }

		private:
			PasContact m_contact;
			std::string m_signQuote;
		};

		class DriSelection : virtual public Decent::Net::CommonJsonMsg
		{
		public:
			static constexpr char const sk_labelDriContact[] = "Contact";
			static constexpr char const sk_labelTripId[] = "TripId";

		public:
			DriSelection() = delete;

			DriSelection(const DriContact& contact, const std::string& tripId) :
				m_contact(contact),
				m_tripId(tripId)
			{}

			DriSelection(DriContact&& contact, std::string&& tripId) :
				m_contact(std::forward<DriContact>(contact)),
				m_tripId(std::forward<std::string>(tripId))
			{}

			DriSelection(const DriSelection& rhs) :
				DriSelection(rhs.m_contact, rhs.m_tripId)
			{}

			DriSelection(DriSelection&& rhs) :
				DriSelection(std::forward<DriContact>(rhs.m_contact),
					std::forward<std::string>(rhs.m_tripId))
			{}

			DriSelection(const JsonValue& json) :
				DriSelection(ParseSubMessage<DriContact>(json, sk_labelDriContact),
					ParseValue<std::string>(json, sk_labelTripId))
			{}

			~DriSelection() {}

			virtual JsonValue& ToJson(JsonDoc& doc) const override;

			const DriContact& GetContact() const { return m_contact; }
			const std::string& GetTripId() const { return m_tripId; }

		private:
			DriContact m_contact;
			std::string m_tripId;
		};

		class PasMatchedResult : virtual public Decent::Net::CommonJsonMsg
		{
		public:
			static constexpr char const sk_labelTripId[] = "TripId";
			static constexpr char const sk_labelDriContact[] = "DriContact";

		public:
			PasMatchedResult() = delete;

			PasMatchedResult(const std::string& tripId, const DriContact& driContact) :
				m_tripId(tripId),
				m_driContact(driContact)
			{}

			PasMatchedResult(std::string&& tripId, DriContact&& driContact) :
				m_tripId(std::forward<std::string>(tripId)),
				m_driContact(std::forward<DriContact>(driContact))
			{}

			PasMatchedResult(const PasMatchedResult& rhs) :
				PasMatchedResult(rhs.m_tripId, rhs.m_driContact)
			{}

			PasMatchedResult(PasMatchedResult&& rhs) :
				PasMatchedResult(std::forward<std::string>(rhs.m_tripId),
					std::forward<DriContact>(rhs.m_driContact))
			{}

			PasMatchedResult(const JsonValue& json) :
				PasMatchedResult(ParseValue<std::string>(json, sk_labelTripId),
					ParseSubMessage<DriContact>(json, sk_labelDriContact))
			{}

			~PasMatchedResult() {}

			virtual JsonValue& ToJson(JsonDoc& doc) const override;

			const DriContact& GetDriContact() const { return m_driContact; }
			const std::string& GetTripId() const { return m_tripId; }

		private:
			DriContact m_driContact;
			std::string m_tripId;
		};

		class PasQueryLog : virtual public Decent::Net::CommonJsonMsg
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
				PasQueryLog(ParseValue<std::string>(json, sk_labelUserId),
					ParseSubMessage<GetQuote>(json, sk_labelGetQuote))
			{}

			~PasQueryLog() {}

			virtual JsonValue& ToJson(JsonDoc& doc) const override;

			const std::string& GetUserId() const { return m_userId; }
			const GetQuote& GetGetQuote() const { return m_getQuote; }

		private:
			std::string m_userId;
			GetQuote m_getQuote;
		};

		class DriverLoc : virtual public Decent::Net::CommonJsonMsg
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
				DriverLoc(ParseSubMessage<Point2D<double> >(json, sk_labelOrigin))
			{}

			~DriverLoc() {}

			virtual JsonValue& ToJson(JsonDoc& doc) const override;

			const Point2D<double>& GetLoc() { return m_loc; }

		private:
			Point2D<double> m_loc;
		};

		class MatchItem : virtual public Decent::Net::CommonJsonMsg
		{
		public:
			static constexpr char const sk_labelTripId[] = "Id";
			static constexpr char const sk_labelPath[] = "Path";

		public:
			MatchItem() = delete;
			MatchItem(const std::string& tripId, const Path& path) :
				m_tripId(tripId),
				m_path(path)
			{}

			MatchItem(std::string&& tripId, Path&& path) :
				m_tripId(std::forward<std::string>(tripId)),
				m_path(std::forward<Path>(path))
			{}

			MatchItem(const MatchItem& rhs) :
				MatchItem(rhs.m_tripId, rhs.m_path)
			{}

			MatchItem(MatchItem&& rhs) :
				MatchItem(std::forward<std::string>(rhs.m_tripId),
					std::forward<Path>(rhs.m_path))
			{}

			MatchItem(const JsonValue& json) :
				MatchItem(ParseValue<std::string>(json, sk_labelTripId),
					ParseSubMessage<Path>(json, sk_labelPath))
			{}

			~MatchItem() {}

			virtual JsonValue& ToJson(JsonDoc& doc) const override;

			const std::string& GetTripId() const { return m_tripId; }
			const Path& GetPath() const { return m_path; }

		private:
			std::string m_tripId;
			Path m_path;
		};

		class BestMatches :virtual public Decent::Net::CommonJsonMsg
		{
		public:
			static constexpr char const sk_labelMatches[] = "Mat";

			static std::vector<MatchItem> ParseMatches(const JsonValue& json);

		public:
			BestMatches() = delete;
			BestMatches(const std::vector<MatchItem>& path) :
				m_matches(path)
			{}

			BestMatches(std::vector<MatchItem>&& path) :
				m_matches(std::forward<std::vector<MatchItem> >(path))
			{}

			BestMatches(const BestMatches& rhs) :
				BestMatches(rhs.m_matches)
			{}

			BestMatches(BestMatches&& rhs) :
				BestMatches(std::forward<std::vector<MatchItem> >(rhs.m_matches))
			{}

			BestMatches(const JsonValue& json) :
				BestMatches(ParseMatches(json))
			{}

			~BestMatches() {}

			virtual JsonValue& ToJson(JsonDoc& doc) const override;

			const std::vector<MatchItem>& GetMatches() const { return m_matches; }

		private:
			std::vector<MatchItem> m_matches;
		};

		class DriQueryLog : virtual public Decent::Net::CommonJsonMsg
		{
		public:
			static constexpr char const sk_labelDriverId[] = "DriverId";
			static constexpr char const sk_labelLoc[] = "Loc";

		public:
			DriQueryLog() = delete;

			DriQueryLog(const std::string& driverId, const Point2D<double>& loc) :
				m_driverId(driverId),
				m_loc(loc)
			{}

			DriQueryLog(std::string&& driverId, Point2D<double>&& loc) :
				m_driverId(std::forward<std::string>(driverId)),
				m_loc(std::forward<Point2D<double> >(loc))
			{}

			DriQueryLog(const DriQueryLog& rhs) :
				DriQueryLog(rhs.m_driverId, rhs.m_loc)
			{}

			DriQueryLog(DriQueryLog&& rhs) :
				DriQueryLog(std::forward<std::string>(rhs.m_driverId),
					std::forward<Point2D<double> >(rhs.m_loc))
			{}

			DriQueryLog(const JsonValue& json) :
				DriQueryLog(ParseValue<std::string>(json, sk_labelDriverId),
					ParseSubMessage<Point2D<double> >(json, sk_labelLoc))
			{}

			~DriQueryLog() {}

			virtual JsonValue& ToJson(JsonDoc& doc) const override;

			const std::string& GetDriverId() const { return m_driverId; }
			const Point2D<double>& GetLoc() const { return m_loc; }

		private:
			std::string m_driverId;
			Point2D<double> m_loc;
		};

		class FinalBill : virtual public Decent::Net::CommonJsonMsg
		{
		public:
			static constexpr char const sk_labelQuote[] = "Quote";
			static constexpr char const sk_labelOpPayment[] = "OpPay";
			static constexpr char const sk_labelDriId[] = "DriId";

		public:
			FinalBill() = delete;

			FinalBill(const Quote& quote, const std::string& opPayment, const std::string& driId) :
				m_quote(quote),
				m_opPay(opPayment),
				m_driId(driId)
			{}

			FinalBill(Quote&& quote, std::string&& opPayment, std::string&& driId) :
				m_quote(std::forward<Quote>(quote)),
				m_opPay(std::forward<std::string>(opPayment)),
				m_driId(std::forward<std::string>(driId))
			{}

			FinalBill(const FinalBill& rhs) :
				FinalBill(rhs.m_quote, rhs.m_opPay, rhs.m_driId)
			{}

			FinalBill(FinalBill&& rhs) :
				FinalBill(std::forward<Quote>(rhs.m_quote),
					std::forward<std::string>(rhs.m_opPay),
					std::forward<std::string>(rhs.m_driId))
			{}

			FinalBill(const JsonValue& json) :
				FinalBill(ParseSubMessage<Quote >(json, sk_labelQuote),
					ParseValue<std::string>(json, sk_labelOpPayment),
					ParseValue<std::string>(json, sk_labelDriId))
			{}

			~FinalBill() {}

			virtual JsonValue& ToJson(JsonDoc& doc) const override;

			const Quote& GetQuote() const { return m_quote; }
			const std::string& GetOpPayment() const { return m_opPay; }
			const std::string& GetDriId() const { return m_driId; }

		private:
			Quote m_quote;
			std::string m_opPay;
			std::string m_driId;
		};

		class PasReg : virtual public Decent::Net::CommonJsonMsg
		{
		public:
			static constexpr char const sk_labelContact[] = "Contact";
			static constexpr char const sk_labelPayment[] = "Pay";
			static constexpr char const sk_labelCsr[] = "Csr";

		public:
			PasReg() = delete;

			PasReg(const PasContact& contact, const std::string& payment, const std::string& csr) :
				m_contact(contact),
				m_pay(payment),
				m_csr(csr)
			{}

			PasReg(PasContact&& contact, std::string&& payment, std::string&& csr) :
				m_contact(std::forward<PasContact>(contact)),
				m_pay(std::forward<std::string>(payment)),
				m_csr(std::forward<std::string>(csr))
			{}

			PasReg(const PasReg& rhs) :
				PasReg(rhs.m_contact, rhs.m_pay, rhs.m_csr)
			{}

			PasReg(PasReg&& rhs) :
				PasReg(std::forward<PasContact>(rhs.m_contact),
					std::forward<std::string>(rhs.m_pay),
					std::forward<std::string>(rhs.m_csr))
			{}

			PasReg(const JsonValue& json) :
				PasReg(ParseSubMessage<PasContact>(json, sk_labelContact),
					ParseValue<std::string>(json, sk_labelPayment),
					ParseValue<std::string>(json, sk_labelCsr))
			{}

			~PasReg() {}

			virtual JsonValue& ToJson(JsonDoc& doc) const override;

			const PasContact& GetContact() const { return m_contact; }
			const std::string& GetPayment() const { return m_pay; }
			const std::string& GetCsr() const { return m_csr; }

		private:
			PasContact m_contact;
			std::string m_pay;
			std::string m_csr;
		};

		class DriReg : virtual public Decent::Net::CommonJsonMsg
		{
		public:
			static constexpr char const sk_labelContact[] = "Contact";
			static constexpr char const sk_labelPayment[] = "Pay";
			static constexpr char const sk_labelCsr[] = "Csr";
			static constexpr char const sk_labelDriLic[] = "Lic";

		public:
			DriReg() = delete;

			DriReg(const DriContact& contact, const std::string& payment, const std::string& csr, const std::string& lic) :
				m_contact(contact),
				m_pay(payment),
				m_csr(csr),
				m_driLic(lic)
			{}

			DriReg(DriContact&& contact, std::string&& payment, std::string&& csr, std::string&& lic) :
				m_contact(std::forward<DriContact>(contact)),
				m_pay(std::forward<std::string>(payment)),
				m_csr(std::forward<std::string>(csr)),
				m_driLic(std::forward<std::string>(lic))
			{}

			DriReg(const DriReg& rhs) :
				DriReg(rhs.m_contact, rhs.m_pay, rhs.m_csr, rhs.m_driLic)
			{}

			DriReg(DriReg&& rhs) :
				DriReg(std::forward<DriContact>(rhs.m_contact),
					std::forward<std::string>(rhs.m_pay),
					std::forward<std::string>(rhs.m_csr),
					std::forward<std::string>(rhs.m_driLic))
			{}

			DriReg(const JsonValue& json) :
				DriReg(ParseSubMessage<DriContact>(json, sk_labelContact),
					ParseValue<std::string>(json, sk_labelPayment),
					ParseValue<std::string>(json, sk_labelCsr),
					ParseValue<std::string>(json, sk_labelDriLic))
			{}

			~DriReg() {}

			virtual JsonValue& ToJson(JsonDoc& doc) const override;

			const DriContact& GetContact() const { return m_contact; }
			const std::string& GetPayment() const { return m_pay; }
			const std::string& GetCsr() const { return m_csr; }
			const std::string& GetDriLic() const { return m_driLic; }

		private:
			DriContact m_contact;
			std::string m_pay;
			std::string m_csr;
			std::string m_driLic;
		};

		class RequestedPayment : virtual public Decent::Net::CommonJsonMsg
		{
		public:
			static constexpr char const sk_labelPayment[] = "Pay";
			static constexpr char const sk_labelOpPaymnet[] = "OpPay";

		public:
			RequestedPayment() = delete;

			RequestedPayment(const std::string& payment, const std::string& opPayment) :
				m_pay(payment),
				m_opPay(opPayment)
			{}

			RequestedPayment(std::string&& payment, std::string&& opPayment) :
				m_pay(std::forward<std::string>(payment)),
				m_opPay(std::forward<std::string>(opPayment))
			{}

			RequestedPayment(const RequestedPayment& rhs) :
				RequestedPayment(rhs.m_pay, rhs.m_opPay)
			{}

			RequestedPayment(RequestedPayment&& rhs) :
				RequestedPayment(std::forward<std::string>(rhs.m_pay),
					std::forward<std::string>(rhs.m_opPay))
			{}

			RequestedPayment(const JsonValue& json) :
				RequestedPayment(ParseValue<std::string>(json, sk_labelPayment),
					ParseValue<std::string>(json, sk_labelOpPaymnet))
			{}

			~RequestedPayment() {}

			virtual JsonValue& ToJson(JsonDoc& doc) const override;

			const std::string& GetPayemnt() const { return m_pay; }
			const std::string& GetOpPayment() const { return m_opPay; }

		private:
			std::string m_pay;
			std::string m_opPay;
		};
	}
}
