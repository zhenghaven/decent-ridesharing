#include "OperatorPayment.h"

namespace
{
	static std::string gs_operatorPayInfo;
}

const std::string & RideShare::OperatorPayment::GetPaymentInfo()
{
	return gs_operatorPayInfo;
}

bool RideShare::OperatorPayment::IsPaymentInfoValid()
{
	return gs_operatorPayInfo.size() > 0;
}

extern "C" void ecall_ride_share_init(const char* pay_info)
{
	gs_operatorPayInfo = pay_info;
}
