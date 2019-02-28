#pragma once

#include <cstdint>
#include <string>

namespace RideShare
{
	namespace EncFunc
	{
		template<typename T>
		inline std::string GetMsgString(const T num)
		{
			return std::string(&num, &num + sizeof(num));
		}

		template<typename T>
		inline T GetNum(const std::string& msg)
		{
			return msg.size() == sizeof(T) ? *reinterpret_cast<const T*>(msg.data()) : -1;
		}

		namespace TripPlaner
		{
			typedef uint8_t NumType;
			constexpr NumType k_getQuote     = 0;
		}

		namespace PassengerMgm
		{
			typedef uint8_t NumType;
			constexpr NumType k_userReg     = 0;
			constexpr NumType k_logQuery    = 1;
			constexpr NumType k_getPayInfo  = 2;
		}

		namespace DriverMgm
		{
			typedef uint8_t NumType;
			constexpr NumType k_userReg     = 0;
			constexpr NumType k_logQuery    = 1;
			constexpr NumType k_getPayInfo  = 2;
		}

		namespace Billing
		{
			typedef uint8_t NumType;
			constexpr NumType k_calPrice = 0;
		}

		namespace TripMatcher
		{
			typedef uint8_t NumType;
			constexpr NumType k_confirmQuote = 0;
			constexpr NumType k_findMatch    = 1;
			constexpr NumType k_confirmMatch = 2;
			constexpr NumType k_tripStart    = 3;
			constexpr NumType k_tripEnd      = 4;
		}

		namespace Payment
		{
			typedef uint8_t NumType;
			constexpr NumType k_procPayment = 0;
		}
	}
}
