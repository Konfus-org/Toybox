#pragma once
#include "Tbx/DllExport.h"
#include <string>

namespace Tbx
{
	struct EXPORT Text
	{
		std::string Value = "";
		std::string Font = "";
		int Size = 12;
	};
}