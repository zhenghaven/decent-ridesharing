#pragma once

#include <DecentApi/Common/Ra/TlsConfig.h>

namespace RideShare
{
	class ClientX509;

	class TlsConfig : public Decent::Ra::TlsConfig
	{
	public:
		TlsConfig(const std::string& expectedVerifierName, Decent::Ra::States& state, bool isServer);

		TlsConfig(TlsConfig&& other) :
			Decent::Ra::TlsConfig(std::forward<TlsConfig>(other))
		{}

		TlsConfig(const TlsConfig& other) = delete;

		virtual ~TlsConfig() {}

		virtual TlsConfig& operator=(const TlsConfig& other) = delete;

		virtual TlsConfig& operator=(TlsConfig&& other)
		{
			Decent::Ra::TlsConfig::operator=(std::forward<Decent::Ra::TlsConfig>(other));

			return *this;
		}

	protected:
		virtual int CertVerifyCallBack(mbedtls_x509_crt& cert, int depth, uint32_t& flag) const override;
		virtual int ClientCertVerifyCallBack(const RideShare::ClientX509& cert, int depth, uint32_t& flag) const;

	private:
	};
}
