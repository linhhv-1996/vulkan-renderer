#pragma once

#include <glm/vec3.hpp>

#include <string>
#include <tuple>

namespace inexor::vulkan_renderer::world {

/// @brief A wrapper for collisions between a ray and octree geometry.
/// This class is used for octree collision, but it can be used for every cube-like data structure
/// @tparam T A templatable type which offers a size() and center() method.
template <typename T>
class RayCubeCollision {
    const T &m_cube;

    using cube_face = std::tuple<std::string, glm::vec3>;
    using cube_corner = std::tuple<std::string, glm::vec3>;
    using cube_edge = std::tuple<std::string, glm::vec3>;

    glm::vec3 m_intersection;
    cube_face m_selected_face;
    cube_corner m_nearest_corner;
    cube_edge m_nearest_edge;

public:
    /// @brief Calculate point of intersection, selected face,
    /// nearest corner on that face, and nearest edge on that face.
    /// @param cube The cube to check for collision.
    /// @param ray_pos The start point of the ray.
    /// @param ray_dir The direction of the ray.
    RayCubeCollision(const T &cube, const glm::vec3 ray_pos, const glm::vec3 ray_dir);

    RayCubeCollision(const RayCubeCollision &) = delete;

    RayCubeCollision(RayCubeCollision &&other) noexcept;

    ~RayCubeCollision() = default;

    RayCubeCollision &operator=(const RayCubeCollision &) = delete;
    RayCubeCollision &operator=(RayCubeCollision &&) = delete;

    [[nodiscard]] const T &cube() const noexcept {
        return m_cube;
    }

    [[nodiscard]] glm::vec3 intersection() const noexcept {
        return m_intersection;
    }

    [[nodiscard]] glm::vec3 face() const noexcept {
        return std::get<1>(m_selected_face);
    }

    [[nodiscard]] const std::string &face_name() const noexcept {
        return std::get<0>(m_selected_face);
    }

    [[nodiscard]] const glm::vec3 &corner() const noexcept {
        return std::get<1>(m_nearest_corner);
    }

    [[nodiscard]] const std::string &corner_name() const noexcept {
        return std::get<0>(m_nearest_corner);
    }

    [[nodiscard]] const glm::vec3 &edge() const noexcept {
        return std::get<1>(m_nearest_edge);
    }

    [[nodiscard]] const std::string &edge_name() const noexcept {
        return std::get<0>(m_nearest_edge);
    }
};

} // namespace inexor::vulkan_renderer::world
