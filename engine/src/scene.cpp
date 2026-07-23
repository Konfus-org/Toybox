#include "runtime_state.h"
#include "tbx/scene.h"
#include "tbx/physics/physics.h"
#include "tbx/platform/input.h"
#include "tbx/platform/window.h"
#include <utility>

// The script-facing engine API over the running runtime. One thin layer so the SAME verbs serve C++
// systems and the generated bindings; every function resolves the world/input/physics through
// tbx::internal::get_runtime() (main thread only).
namespace tbx
{
    Toy spawn(std::string name)
    {
        return internal::get_runtime().sandbox.add(std::move(name));
    }

    std::optional<Toy> find(std::string name)
    {
        return internal::get_runtime().sandbox.find(std::string_view(name));
    }

    void despawn(Toy toy)
    {
        internal::get_runtime().sandbox.remove(toy);
    }

    std::vector<Toy> toys()
    {
        return internal::get_runtime().sandbox.get_toys();
    }

    bool is_key_down(Key key)
    {
        return is_down(internal::get_runtime().input, key);
    }

    bool is_key_pressed(Key key)
    {
        return is_pressed(internal::get_runtime().input, key);
    }

    bool is_key_released(Key key)
    {
        return is_released(internal::get_runtime().input, key);
    }

    bool is_mouse_down(MouseButton button)
    {
        return is_down(internal::get_runtime().input, button);
    }

    bool is_mouse_pressed(MouseButton button)
    {
        return is_pressed(internal::get_runtime().input, button);
    }

    bool is_mouse_released(MouseButton button)
    {
        return is_released(internal::get_runtime().input, button);
    }

    float get_mouse_axis(MouseAxis axis)
    {
        return get_axis(internal::get_runtime().input, axis);
    }

    std::optional<RaycastHit> raycast(const Vec3& origin, const Vec3& direction, float max_distance)
    {
        return internal::raycast(internal::get_runtime().physics, origin, direction, max_distance);
    }

    const std::vector<Window>& get_open_windows()
    {
        return internal::get_runtime().windows.open_windows;
    }
}
