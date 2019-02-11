#include "Crypto.h"

using namespace RideShare;
using namespace Decent::MbedTlsObj;
using namespace Decent::Ra;

ClientX509::ClientX509(const ECKeyPublic & pub, 
	const Decent::Ra::AppX509 & verifierCert, const ECKeyPair & verifierPrvKey, const std::string & userName) :
	Decent::Ra::AppX509(pub, verifierCert, verifierPrvKey, userName, "Client", "N/A", "")
{
}
