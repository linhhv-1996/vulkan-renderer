#include <benchmark/benchmark.h>

#include <inexor/vulkan-renderer/world/collision_query.hpp>
#include <inexor/vulkan-renderer/world/cube.hpp>

namespace inexor::vulkan_renderer {

static void BM_CubeCollision(benchmark::State &state) {
    for (auto _ : state) {
        const glm::vec3 world_pos{0, 0, 0};
        world::Cube world(world::Cube::Type::SOLID, 1.0f, world_pos);

        glm::vec3 cam_pos{0.0f, 0.0f, 10.0f};
        glm::vec3 cam_direction{0.0f, 0.0f, -1.0f};

        world::OctreeCollisionQuery collision_check(world);
        auto collision = collision_check.check_for_collision(cam_pos, cam_direction);
    }
}

BENCHMARK(BM_CubeCollision);

}; // namespace inexor::vulkan_renderer
