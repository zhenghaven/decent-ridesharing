#pragma once

#include <DecentApi/Common/Ra/Crypto.h>

namespace RideShare
{
	class ClientX509 : public Decent::Ra::AppX509
	{
	public:
		ClientX509() = delete;

		ClientX509(const std::string & pemStr) :
			Decent::Ra::AppX509(pemStr)
		{}

		ClientX509(mbedtls_x509_crt& cert) :
			Decent::Ra::AppX509(cert)
		{}

		ClientX509(const Decent::MbedTlsObj::ECKeyPublic& pub,
			const Decent::Ra::AppX509& verifierCert, const Decent::MbedTlsObj::ECKeyPair& verifierPrvKey,
			const std::string& userName, const std::string& clientInfo);

		ClientX509(ClientX509&& other) :
			Decent::Ra::AppX509(std::forward<Decent::Ra::AppX509>(other))
		{}

		ClientX509(const ClientX509& other) = delete;

		virtual ~ClientX509() {}

		virtual ClientX509& operator=(ClientX509&& other)
		{
			Decent::Ra::AppX509::operator=(std::forward<Decent::Ra::AppX509>(other));
			return *this;
		}
	};

}
