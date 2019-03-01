#pragma once

#include <DecentApi/DecentAppApp/DecentApp.h>

namespace RideShare
{
	class RideShareApp : public Decent::RaSgx::DecentApp
	{
	public:
		RideShareApp() = delete;
		RideShareApp(const std::string& enclavePath, const std::string& tokenPath, const std::string& wListKey, Decent::Net::Connection& serverConn, const std::string& opPayInfo);
		RideShareApp(const fs::path& enclavePath, const fs::path& tokenPath, const std::string& wListKey, Decent::Net::Connection& serverConn, const std::string& opPayInfo);
		RideShareApp(const std::string& enclavePath, const Decent::Tools::KnownFolderType tokenLocType, const std::string& tokenFileName, const std::string& wListKey, Decent::Net::Connection& serverConn, const std::string& opPayInfo);

		virtual ~RideShareApp() {}

	private:
		void InitEnclave(const std::string& opPayInfo);

	};
}

