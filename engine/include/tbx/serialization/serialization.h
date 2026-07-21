#pragma once
// The one serialization seam: the selected backend (cmake tbx_backend(SERIALIZATION ...))
// provides tbx::Json plus parse_json/dump_json/is_valid_json. Nothing else names the library.
#include <tbx_serialization_backend.h>
