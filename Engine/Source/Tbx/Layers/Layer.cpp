#include "Tbx/PCH.h"
#include "Tbx/Layers/Layer.h"

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