#pragma once

#include <string>

namespace RideShare
{
	namespace OperatorPayment
	{
		const std::string& GetPaymentInfo();
		bool IsPaymentInfoValid();
	}
}
