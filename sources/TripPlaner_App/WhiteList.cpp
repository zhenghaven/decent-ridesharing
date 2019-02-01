#include <DecentApi/CommonApp/Ra/WhiteList/Requester.h>

using namespace Decent::Ra::WhiteList;

Requester::Requester() :
	m_key("TestHashList_01"),
	m_whiteList{
	std::make_pair("TestAppHash", "TestAppName"),
}
{
}
