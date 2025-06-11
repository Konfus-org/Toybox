#include "Tbx/Application/PCH.h"
#include "Tbx/Application/Layers/Layer.h"

namespace Tbx 
{
	Layer::Layer(const std::string& name)
	{
		_name = name;
	}

	std::string Layer::GetName() const
	{
		return _name;
	}
}