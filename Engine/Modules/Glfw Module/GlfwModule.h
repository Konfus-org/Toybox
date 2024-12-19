#pragma once
#include <vector>
#include <Core.h>

extern "C" TBX_MODULE_API std::vector<Toybox::Module*>* LoadMultiple();
extern "C" TBX_MODULE_API void Unload();
