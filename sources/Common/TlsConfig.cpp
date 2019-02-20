#include "TlsConfig.h"

#include <mbedtls/ssl.h>
#include <DecentApi/Common/Common.h>

#include "Crypto.h"

using namespace RideShare;

namespace
{
	static constexpr int MBEDTLS_SUCCESS_RET = 0;
}

TlsConfig::TlsConfig(const std::string & expectedVerifierName, Decent::Ra::States& state, bool isServer) :
	Decent::Ra::TlsConfig(expectedVerifierName, state, isServer)
{
}

int TlsConfig::CertVerifyCallBack(mbedtls_x509_crt& cert, int depth, uint32_t& flag) const
{
	switch (depth)
	{
	case 0: //Client Cert
	{
		RideShare::ClientX509 certObj(cert);
		LOGI("Verifing Client Cert: %s.", certObj.GetCommonName().c_str());
		if (!certObj)
		{
			flag = MBEDTLS_X509_BADCERT_NOT_TRUSTED;
			return MBEDTLS_SUCCESS_RET;
		}
		return ClientCertVerifyCallBack(certObj, depth, flag);
	}
	case 1: //Verifier Cert
	{
		Decent::Ra::AppX509 certObj(cert);
		LOGI("Verifing App Cert: %s.", certObj.GetCommonName().c_str());
		if (!certObj)
		{
			flag = MBEDTLS_X509_BADCERT_NOT_TRUSTED;
			return MBEDTLS_SUCCESS_RET;
		}
		return AppCertVerifyCallBack(certObj, depth, flag);
	}
	case 2: //Decent Cert
	{
		const Decent::Ra::ServerX509 serverCert(cert);
		LOGI("Verifing Server Cert: %s.", serverCert.GetCommonName().c_str());
		if (!serverCert)
		{
			flag = MBEDTLS_X509_BADCERT_NOT_TRUSTED;
			return MBEDTLS_SUCCESS_RET;
		}
		return ServerCertVerifyCallBack(serverCert, depth, flag);
	}
	default:
		return MBEDTLS_ERR_X509_FATAL_ERROR;
	}
}

int TlsConfig::ClientCertVerifyCallBack(const RideShare::ClientX509& cert, int depth, uint32_t& flag) const
{
	if (flag != MBEDTLS_SUCCESS_RET)
	{//client cert is invalid!
		return MBEDTLS_SUCCESS_RET;
	}

	return MBEDTLS_SUCCESS_RET;
}
