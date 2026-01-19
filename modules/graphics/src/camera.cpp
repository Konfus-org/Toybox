#include "tbx/graphics/camera.h"
#include "tbx/graphics/frustum.h"
#include "tbx/math/matrices.h"
#include "tbx/math/trig.h"
#include <numbers>

namespace tbx
{
    Camera::Camera()
    {
        set_perspective(_fov, _aspect, _z_near, _z_far);
    }

    void Camera::set_orthographic(float size, float aspect, float z_near, float z_far)
    {
        const float half_size = size * 0.5f;
        const float top = half_size;
        const float bottom = -half_size;
        const float right = top * aspect;
        const float left = -right;

        _is_perspective = false;
        _z_near = z_near;
        _z_far = z_far;
        _fov = size;
        _aspect = aspect;
        _projection_matrix = ortho_projection(left, right, bottom, top, z_near, z_far);
    }

    void Camera::set_perspective(float fov, float aspect, float z_near, float z_far)
    {
        _is_perspective = true;
        _z_near = z_near;
        _z_far = z_far;
        _aspect = aspect;
        _fov = fov;
        _projection_matrix = perspective_projection(degrees_to_radians(fov), aspect, z_near, z_far);
    }

    void Camera::set_aspect(float aspect)
    {
        if (_aspect == aspect)
            return;

        if (_is_perspective)
            set_perspective(_fov, aspect, _z_near, _z_far);
        else
            set_orthographic(_fov, aspect, _z_near, _z_far);
    }

    bool Camera::is_perspective() const
    {
        return _is_perspective;
    }

    bool Camera::is_orthographic() const
    {
        return !_is_perspective;
    }

    float Camera::get_aspect() const
    {
        return _aspect;
    }

    float Camera::get_fov() const
    {
        return _fov;
    }

    float Camera::get_z_near() const
    {
        return _z_near;
    }

    float Camera::get_z_far() const
    {
        return _z_far;
    }

    const Mat4& Camera::get_projection_matrix() const
    {
        return _projection_matrix;
    }

    Mat4 Camera::get_view_matrix(const Vec3& camera_position, const Quat& camera_rotation)
    {
        const Mat4 rotation_matrix = mat4_cast(camera_rotation);
        const Mat4 inverse_rotation_matrix = inverse(rotation_matrix);
        const Mat4 translation_matrix = translate(Mat4(1.0f), -camera_position);
        return inverse_rotation_matrix * translation_matrix;
    }

    Mat4 Camera::get_view_projection_matrix(
        const Vec3& camera_position,
        const Quat& camera_rotation)
    {
        const Mat4 view_matrix = get_view_matrix(camera_position, camera_rotation);
        return get_projection_matrix() * view_matrix;
    }

    Frustum Camera::get_frustum(const Vec3& camera_position, const Quat& camera_rotation)
    {
        const Mat4 view_projection = get_view_projection_matrix(camera_position, camera_rotation);
        return Frustum(view_projection);
    }

}
