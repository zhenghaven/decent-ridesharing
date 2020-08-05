#pragma once

#include <DecentApi/DecentAppApp/DecentApp.h>

namespace RideShare
{
	class RideShareApp : public Decent::RaSgx::DecentApp
	{
	public:
		RideShareApp() = delete;

		RideShareApp(const std::string& enclavePath, const std::string& tokenPath, const std::string& wListKey, Decent::Net::ConnectionBase& serverConn,
			const std::string& opPayInfo);

		RideShareApp(const fs::path& enclavePath, const fs::path& tokenPath, const std::string& wListKey, Decent::Net::ConnectionBase& serverConn,
			const std::string& opPayInfo);

		RideShareApp(const std::string& enclavePath, const std::string& tokenPath,
			const size_t numTWorker, const size_t numUWorker, const size_t retryFallback, const size_t retrySleep,
			const std::string& wListKey, Decent::Net::ConnectionBase& serverConn,
			const std::string& opPayInfo);

		RideShareApp(const fs::path& enclavePath, const fs::path& tokenPath,
			const size_t numTWorker, const size_t numUWorker, const size_t retryFallback, const size_t retrySleep,
			const std::string& wListKey, Decent::Net::ConnectionBase& serverConn,
			const std::string& opPayInfo);


		virtual ~RideShareApp() {}

	private:
		void InitEnclave(const std::string& opPayInfo);

	};
}

