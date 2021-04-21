#include "inexor/vulkan-renderer/world/collision_query.hpp"

#include <inexor/vulkan-renderer/world/cube.hpp>

#include <glm/gtx/intersect.hpp>

namespace inexor::vulkan_renderer::world {

OctreeCollisionQuery::OctreeCollisionQuery(const Cube &cube) : m_cube(cube) {}

bool OctreeCollisionQuery::ray_box_collision(const std::array<glm::vec3, 2> box_bounds, const glm::vec3 position,
                                             const glm::vec3 direction) const {
    glm::vec3 inverse_dir{1 / direction.x, 1 / direction.y, 1 / direction.z};
    std::int32_t sign[3]{inverse_dir.x < 0, inverse_dir.y < 0, inverse_dir.z < 0};

    float tmin, tmax, tymin, tymax, tzmin, tzmax;

    tmin = (box_bounds[sign[0]].x - position.x) * inverse_dir.x;
    tmax = (box_bounds[1 - sign[0]].x - position.x) * inverse_dir.x;
    tymin = (box_bounds[sign[1]].y - position.y) * inverse_dir.y;
    tymax = (box_bounds[1 - sign[1]].y - position.y) * inverse_dir.y;

    if ((tmin > tymax) || (tymin > tmax)) {
        return false;
    }
    if (tymin > tmin) {
        tmin = tymin;
    }
    if (tymax < tmax) {
        tmax = tymax;
    }

    tzmin = (box_bounds[sign[2]].z - position.z) * inverse_dir.z;
    tzmax = (box_bounds[1 - sign[2]].z - position.z) * inverse_dir.z;

    return !((tmin > tzmax) || (tzmin > tmax));
}

std::optional<RayCubeCollision<Cube>> OctreeCollisionQuery::check_for_collision(const glm::vec3 pos,
                                                                                const glm::vec3 dir) {
    // If the cube is empty, a collision with a ray is not possible,
    // and there are no sub cubes to check for collision either.
    if (m_cube.type() == Cube::Type::EMPTY) {
        // No collision found.
        return std::nullopt;
    }

    // We need to pass this into glm::intersectRaySphere by reference,
    // although we are not interested into it.
    auto intersection_distance{0.0f};

    const float sphere_radius_squared = static_cast<float>(std::pow(m_cube.bounding_box_radius(), 2));

    // First, check if ray collides with bounding sphere.
    // This is much easier to calculate than a collision with a bounding box.
    if (!glm::intersectRaySphere(pos, dir, m_cube.center(), sphere_radius_squared, intersection_distance)) {
        // No collision found.
        return std::nullopt;
    }

    // Second, check if ray collides with bounding box.
    // This again is also much faster than to check for collision with every one of the 8 sub cubes.
    // TODO: This is an axis aligned bounding box! Alignment must account for rotation in the future!
    if (!ray_box_collision(m_cube.bounding_box(), pos, dir)) {
        // No collision found.
        return std::nullopt;
    }

    if (m_cube.is_leaf()) {
        // We found a leaf collision. Now we need to determine the selected face,
        // nearest corner to intersection point and nearest edge to intersection point.
        return std::make_optional<RayCubeCollision<Cube>>(m_cube, pos, dir);
    } else {
        std::size_t hit_candidate_count{0};
        std::size_t collision_subcube_index{0};
        float m_nearest_square_distance = std::numeric_limits<float>::max();
        auto subcubes = m_cube.childs();

        // Iterate through all sub cubes and check for collision.
        for (std::int32_t i = 0; i < 8; i++) {
            if (subcubes[i]->type() != Cube::Type::EMPTY) {
                OctreeCollisionQuery subcollision(*subcubes[i]);
                if (subcollision.check_for_collision(pos, dir)) {
                    hit_candidate_count++;

                    // If a ray collides with an octant, it can collide with multiple child cubes as it goes
                    // through it. We need to find the cube which is nearest to the camera and also in front of
                    // the camera.
                    const auto distance = subcubes[i]->squared_distance(pos);

                    if (distance < m_nearest_square_distance) {
                        collision_subcube_index = i;
                        m_nearest_square_distance = distance;
                    }
                }
            }

            // If a ray goes through a cube of 8 subcubes, no more than 4 collisions can take place.
            if (hit_candidate_count == 4) {
                break;
            }
        }

        return std::make_optional<RayCubeCollision<Cube>>(*subcubes[collision_subcube_index], pos, dir);
    }

    // No collision found.
    return std::nullopt;
}

}; // namespace inexor::vulkan_renderer::world
