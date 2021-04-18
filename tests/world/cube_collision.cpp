#include <gtest/gtest.h>

#include <inexor/vulkan-renderer/world/collision_query.hpp>
#include <inexor/vulkan-renderer/world/cube.hpp>

namespace inexor::vulkan_renderer {

TEST(CubeCollision, CollisionCheck) {
    const glm::vec3 world_pos{0, 0, 0};
    world::Cube world(world::Cube::Type::SOLID, 1.0f, world_pos);

    glm::vec3 cam_pos{0.0f, 0.0f, 10.0f};
    glm::vec3 cam_direction{0.0f, 0.0f, 0.0f};

    world::OctreeCollisionQuery collision_check(world);
    auto collision1 = collision_check.check_for_collision(cam_pos, cam_direction);
    bool collision_found = collision1.has_value();

    // There must be no collision for this data setup.
    ASSERT_EQ(collision_found, false);

    cam_direction = {0.0f, 0.0f, -1.0f};

    auto collision2 = collision_check.check_for_collision(cam_pos, cam_direction);
    collision_found = collision2.has_value();

    // Since we are now directly looking down on the cube, we collide with it.
    ASSERT_EQ(collision_found, true);
}

} // namespace inexor::vulkan_renderer
