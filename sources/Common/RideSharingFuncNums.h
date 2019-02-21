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
			constexpr NumType k_confirmQuote = 1;
		}

		namespace PassengerMgm
		{
			typedef uint8_t NumType;
			constexpr NumType k_userReg     = 0;
			constexpr NumType k_logQuery    = 1;
			constexpr NumType k_getPayInfo  = 1;
		}
	}
}