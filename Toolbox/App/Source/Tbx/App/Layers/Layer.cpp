#include "Tbx/App/PCH.h"
#include "Tbx/App/Layers/Layer.h"

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