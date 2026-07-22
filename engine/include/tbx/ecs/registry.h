#pragma once
// The one ECS-storage seam: the selected backend (cmake tbx_backend(ECS ...)) supplies the
// registry types via its <tbx_ecs_backend.h>. Nothing outside that header names the ECS
// library; swapping it means re-pointing the backend and keeping these aliases' semantics.
#include <tbx_ecs_backend.h>
