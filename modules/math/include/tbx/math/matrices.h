#pragma once
#ifndef GLM_ENABLE_EXPERIMENTAL
    #define GLM_ENABLE_EXPERIMENTAL
#endif
#include "tbx/math/quaternions.h"
#include "tbx/math/transform.h"
#include "tbx/math/vectors.h"
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtx/quaternion.hpp>

namespace tbx
{
    // Represents a 2x2 floating-point matrix compatible with GLM operations.
    // Ownership: value type; callers own any copies created from this alias.
    // Thread Safety: immutable value semantics; safe for concurrent use when not shared mutably.
    using Mat2 = glm::mat2;

    // Represents a 3x3 floating-point matrix compatible with GLM operations.
    // Ownership: value type; callers own any copies created from this alias.
    // Thread Safety: immutable value semantics; safe for concurrent use when not shared mutably.
    using Mat3 = glm::mat3;

    // Represents a 4x4 floating-point matrix compatible with GLM operations.
    // Ownership: value type; callers own any copies created from this alias.
    // Thread Safety: immutable value semantics; safe for concurrent use when not shared mutably.
    using Mat4 = glm::mat4;

    // Builds an orthographic projection matrix with the provided clip-space bounds.
    // Ownership: returns a matrix by value; the caller owns the copy.
    // Thread Safety: stateless wrapper; safe to call concurrently.
    inline Mat4 ortho_projection(
        float left,
        float right,
        float bottom,
        float top,
        float z_near,
        float z_far)
    {
        return glm::ortho(left, right, bottom, top, z_near, z_far);
    }

    // Builds a perspective projection matrix using a field of view specified in radians.
    // Ownership: returns a matrix by value; the caller owns the copy.
    // Thread Safety: stateless wrapper; safe to call concurrently.
    inline Mat4 perspective_projection(float fov_radians, float aspect, float z_near, float z_far)
    {
        return glm::perspective(fov_radians, aspect, z_near, z_far);
    }

    // Converts a quaternion into its equivalent rotation matrix.
    // Ownership: returns a matrix by value; the caller owns the copy.
    // Thread Safety: stateless wrapper; safe to call concurrently.
    inline Mat4 quaternion_to_mat4(const Quat& rotation)
    {
        return glm::toMat4(rotation);
    }

    // Computes the inverse of a 4x4 matrix.
    // Ownership: returns a matrix by value; the caller owns the copy.
    // Thread Safety: stateless wrapper; safe to call concurrently.
    inline Mat4 inverse(const Mat4& matrix)
    {
        return glm::inverse(matrix);
    }

    /// <summary>
    /// Purpose: Computes the inverse-transpose of a 3x3 matrix.
    /// Ownership: Returns a matrix by value; the caller owns the copy.
    /// Thread Safety: Stateless wrapper; safe to call concurrently.
    /// </summary>
    inline Mat3 inverse_transpose(const Mat3& matrix)
    {
        return glm::inverseTranspose(matrix);
    }

    // Builds a view matrix from position, target, and up vectors.
    // Ownership: returns a matrix by value; the caller owns the copy.
    // Thread Safety: stateless wrapper; safe to call concurrently.
    inline Mat4 look_at(const Vec3& position, const Vec3& target, const Vec3& up)
    {
        return glm::lookAt(position, target, up);
    }

    // Builds a translation matrix from the given vector.
    // Ownership: returns a matrix by value; the caller owns the copy.
    // Thread Safety: stateless wrapper; safe to call concurrently.
    inline Mat4 translate(const Vec3& translation)
    {
        return glm::translate(Mat4(1.0f), translation);
    }

    // Builds a rotation matrix from a quaternion around a specific axis.
    // Ownership: returns a matrix by value; the caller owns the copy.
    // Thread Safety: stateless wrapper; safe to call concurrently.
    inline Mat4 rotate(float angle_radians, const Vec3& axis)
    {
        return glm::rotate(Mat4(1.0f), angle_radians, axis);
    }

    // Builds a scaling matrix from a vector of scale factors.
    // Ownership: returns a matrix by value; the caller owns the copy.
    // Thread Safety: stateless wrapper; safe to call concurrently.
    inline Mat4 scale(const Vec3& factors)
    {
        return glm::scale(Mat4(1.0f), factors);
    }

    /// <summary>Builds a transform matrix from a position/rotation/scale triple.</summary>
    /// <remarks>
    /// Purpose: Composes translation, rotation, and scale into a single matrix.
    /// Ownership: Returns a matrix by value; the caller owns the copy.
    /// Thread Safety: Stateless wrapper; safe to call concurrently.
    /// </remarks>
    inline Mat4 build_transform_matrix(const Transform& transform)
    {
        Mat4 translation = translate(transform.position);
        Mat4 rotation = quaternion_to_mat4(transform.rotation);
        Mat4 scaling = scale(transform.scale);
        return translation * rotation * scaling;
    }
}
