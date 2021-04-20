#pragma once

#include "inexor/vulkan-renderer/world/collision.hpp"

// Forward declaration
namespace inexor::vulkan_renderer::world {
class Cube;
} // namespace inexor::vulkan_renderer::world

#include <glm/vec3.hpp>

#include <optional>

namespace inexor::vulkan_renderer::world {

// TODO: Implement PointCubeCollision
class OctreeCollisionQuery {
    const Cube &m_cube;

    /// @brief ``True`` if the ray collides with the cube's bounding sphere.
    /// @param pos The start position of the ray.
    /// @param dir The direction of the ray.
    /// @return ``True`` if the ray collides with the octree cube's bounding sphere.
    [[nodiscard]] bool ray_sphere_collision(glm::vec3 pos, glm::vec3 dir) const;

    /// @brief ``True`` of the ray build from the two vectors collides with the cube's bounding box.
    /// @note There is no such function as glm::intersectRayBox.
    /// @param box_bounds An array of two vectors which represent the edges of the bounding box.
    /// @param pos The start position of the ray.
    /// @param dir The direction of the ray.
    /// @return ``True`` if the ray collides with the octree cube's bounding box.
    [[nodiscard]] bool ray_box_collision(std::array<glm::vec3, 2> box_bounds, glm::vec3 pos, glm::vec3 dir) const;

public:
    /// @brief Create a new octree collision query
    /// @param world The octree world to check in this collision.
    OctreeCollisionQuery(const Cube &world);

    /// @brief Check for a collision between a camera ray and octree geometry.
    /// @param pos The camera position.
    /// @param dir The camera view direction.
    /// @return A std::optional which contains the collision data (if any found).
    std::optional<RayCubeCollision<Cube>> check_for_collision(glm::vec3 pos, glm::vec3 dir);
};

} // namespace inexor::vulkan_renderer::world
