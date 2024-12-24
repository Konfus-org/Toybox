#include "tbxpch.h"
#include "Layer.h"

namespace Toybox 
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