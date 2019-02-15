#include <cstdint>

namespace RideShare
{
	namespace EncFunc
	{
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
