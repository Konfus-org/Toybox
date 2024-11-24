#include "tbxpch.h"
#include "Layer.h"

namespace Toybox::Layers 
{
	Layer::Layer(const std::string& name)
	{
		_name = name;
	}

	const std::string Layers::Layer::GetName() const
	{
		return _name;
	}
}