#include "Tools.h"

#include <boost/filesystem.hpp>

#include <DecentApi/CommonApp/Tools/DiskFile.h>

using namespace Decent::Tools;

bool RideShare::Tools::GetConfigurationJsonString(const std::string & filePath, std::string & outJsonStr)
{
	DiskFile file(filePath, FileBase::Mode::Read, FileBase::sk_deferOpen);

	try
	{
		file.Open();
		outJsonStr.resize(file.GetFileSize());
		file.ReadBlockExactSize(outJsonStr);
		return true;
	}
	catch (const FileException&)
	{
		return false;
	}
}
