#include <DecentApi/Common/Ra/States.h>

#include <DecentApi/Common/Ra/KeyContainer.h>
#include <DecentApi/Common/Ra/CertContainer.h>
#include <DecentApi/Common/Ra/WhiteList/Loaded.h>
#include <DecentApi/Common/Ra/WhiteList/HardCoded.h>
#include <DecentApi/Common/Ra/WhiteList/DecentServer.h>

using namespace Decent::Ra;

namespace
{
	static CertContainer certContainer;
	static KeyContainer keyContainer;
	static WhiteList::DecentServer serverWhiteList;
	static WhiteList::HardCoded hardCodedWhiteList;

	static const WhiteList::Loaded& GetLoadedWhiteListImpl(AppX509* certPtr)
	{
		static WhiteList::Loaded inst(
			{
				std::make_pair("TestAppHash", "TestAppName"),
			}
		);
		return inst;
	}
}

States::States() :
	m_certContainer(certContainer),
	m_serverWhiteList(serverWhiteList),
	m_hardCodedWhiteList(hardCodedWhiteList),
	m_keyContainer(keyContainer),
	m_getLoadedFunc(&GetLoadedWhiteListImpl)
{
}
