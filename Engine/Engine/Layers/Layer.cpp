#include "tbxpch.h"
#include "Layer.h"

namespace Toybox 
{
	Layer::Layer(const std::string& name)
	{
		_name = name;
	}

	const std::string Layer::GetName() const
	{
		return _name;
	}
}