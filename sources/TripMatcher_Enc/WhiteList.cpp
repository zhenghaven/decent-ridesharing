#include <DecentApi/Common/Ra/WhiteList/HardCoded.h>

using namespace Decent::Ra::WhiteList;

HardCoded::HardCoded() :
	StaticTypeList(WhiteListType({
		std::make_pair("oAOdvFKiux89iqcybpLkXly7oAZIvsfeGxh8QqAwT6k=", sk_nameDecentServer)
		}))
{
}
