#include "Tbx/App/Layers/Layer.h"
#include <Tbx/Core/PCH.h>

namespace Tbx 
{
	Layer::Layer(const std::string_view& name)
	{
		_name = name;
	}

	std::string Layer::GetName() const
	{
		return _name;
	}
}