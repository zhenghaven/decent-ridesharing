//#include <memory>
//
//#include <mbedtls/ssl.h>
//
//#include "../common/Net/TlsCommLayer.h"
//#include "../common/MbedTls/MbedTlsObjects.h"
//#include "../common/MbedTls/MbedTlsHelpers.h"
//#include "../common/Ra/States.h"
//#include "../common/Ra/Crypto.h"
//#include "../common/Ra/KeyContainer.h"
//#include "../common/Ra/CertContainer.h"
//
//#include "../common/Common.h"
//
//using namespace Decent;
//using namespace Decent::Ra;
//
//static constexpr char const voteAppCaStr[] = "-----BEGIN CERTIFICATE-----\n\
//MIIBdTCCARqgAwIBAgILAJLcv58ab0wEwA4wDAYIKoZIzj0EAwIFADAiMSAwHgYD\n\
//VQQDExdEZWNlbnQgVm90ZSBBcHAgUm9vdCBDQTAgFw0xODEwMjIxMDMyMjFaGA8y\n\
//MDg2MTEwOTEzNDYyOFowIjEgMB4GA1UEAxMXRGVjZW50IFZvdGUgQXBwIFJvb3Qg\n\
//Q0EwWTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAARiHmZy/OFVfF82RUGKQ/24CQH/\n\
//Zb3pcEg1enLmyzH3TLNPTa11v2KuWyu5+t46eBg5B/YH0b0o6HpzzxnsbOQCozMw\n\
//MTAMBgNVHRMEBTADAQH/MA4GA1UdDwEB/wQEAwIBzjARBglghkgBhvhCAQEEBAMC\n\
//AMQwDAYIKoZIzj0EAwIFAANHADBEAiBvii+PgcSb13JBHPSD2FPIOmMxI6TeHDrp\n\
//tNv/Ivr6DgIgJQ5ZX3y8zE+dIf/Ox+lAGZrNnoqgOyZrmi0OD29ssCw=\n\
//-----END CERTIFICATE-----";
//
//static const MbedTlsObj::X509Cert voteAppCa(voteAppCaStr);
//
//static MbedTlsObj::TlsConfig ConstructTlsConfig(const MbedTlsObj::ECKeyPair& prvKey, const AppX509& appCert)
//{
//	MbedTlsObj::TlsConfig config(new mbedtls_ssl_config);
//	config.BasicInit();
//
//	if (mbedtls_ssl_config_defaults(config.Get(), MBEDTLS_SSL_IS_SERVER,
//			MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_SUITEB) != 0)
//	{
//		config.Destroy();
//		return config;
//	}
//
//	if (prvKey && appCert &&
//		mbedtls_ssl_conf_own_cert(config.Get(), appCert.Get(), prvKey.Get()) != 0)
//	{
//		config.Destroy();
//		return config;
//	}
//
//	mbedtls_ssl_conf_ca_chain(config.Get(), voteAppCa.Get(), nullptr);
//	mbedtls_ssl_conf_authmode(config.Get(), MBEDTLS_SSL_VERIFY_REQUIRED);
//
//	return config;
//}
//
//extern "C" int ecall_vote_app_proc_voter_msg(void* connection)
//{
//	std::shared_ptr<const MbedTlsObj::ECKeyPair> prvKey = States::Get().GetKeyContainer().GetSignKeyPair();
//	std::shared_ptr<const AppX509> appCert = std::dynamic_pointer_cast<const AppX509>(States::Get().GetCertContainer().GetCert());
//
//	std::shared_ptr<const MbedTlsObj::TlsConfig> config(std::make_shared<MbedTlsObj::TlsConfig>(ConstructTlsConfig(*prvKey, *appCert)));
//	Decent::Net::TlsCommLayer testTls(connection, config, true);
//	//testTls.ReceiveMsg(connectionPtr, testMsg);
//	LOGI("Handshake was %s.\n", testTls ? "SUCCESS" : "FAILED");
//
//	std::string voteBuf;
//	testTls.ReceiveMsg(connection, voteBuf);
//	LOGI("Receive vote: %d.\n", voteBuf[0]);
//
//	return true;
//}
