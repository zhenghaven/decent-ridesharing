#pragma once

#include <string>

namespace RideShare
{
	namespace Tools
	{
		bool GetConfigurationJsonString(const std::string& filePath, std::string& outJsonStr);
	}
}
