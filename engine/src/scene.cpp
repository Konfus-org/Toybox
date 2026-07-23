#include "tbx/scene.h"
#include "tbx/physics/physics.h"
#include "tbx/platform/input.h"
#include "tbx/platform/window.h"
#include "tbx/runtime.h"
#include <utility>

// The script-facing engine API over the running runtime. One thin layer so the SAME verbs serve C++
// systems and the generated bindings; every function resolves the world/input/physics through
// tbx::current() (main thread only).
namespace tbx
{
    Toy spawn(std::string name)
    {
        return current().sandbox.add(std::move(name));
    }

    std::optional<Toy> find(std::string name)
    {
        return current().sandbox.find(std::string_view(name));
    }

    void despawn(Toy toy)
    {
        current().sandbox.remove(toy);
    }

    std::vector<Toy> toys()
    {
        return current().sandbox.get_toys();
    }

    bool is_key_down(Key key)
    {
        return is_down(current().input, key);
    }

    bool is_key_pressed(Key key)
    {
        return is_pressed(current().input, key);
    }

    bool is_key_released(Key key)
    {
        return is_released(current().input, key);
    }

    bool is_mouse_down(MouseButton button)
    {
        return is_down(current().input, button);
    }

    bool is_mouse_pressed(MouseButton button)
    {
        return is_pressed(current().input, button);
    }

    bool is_mouse_released(MouseButton button)
    {
        return is_released(current().input, button);
    }

    float get_mouse_axis(MouseAxis axis)
    {
        return get_axis(current().input, axis);
    }

    std::optional<RaycastHit> raycast(const Vec3& origin, const Vec3& direction, float max_distance)
    {
        return internal::raycast(current().physics, origin, direction, max_distance);
    }

    const std::vector<Window>& get_open_windows()
    {
        return current().windows.open_windows;
    }
}
