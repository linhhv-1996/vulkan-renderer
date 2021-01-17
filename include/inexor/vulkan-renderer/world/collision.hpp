#pragma once

#include <inexor/vulkan-renderer/world/cube.hpp>

#include <glm/geometric.hpp>
#include <glm/gtx/vector_angle.hpp>
#include <glm/vec3.hpp>

#include <array>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

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
    RayCubeCollision(const T &cube, const glm::vec3 ray_pos, const glm::vec3 ray_dir) : m_cube(cube) {

        // This lambda adjusts the center points on a cube's face to the size of the octree,
        // so collision works with cubes of any size. This does not yet account for rotations!
        const auto adjust_coordinates = [=](const glm::vec3 pos) {
            // TODO: Take rotation of the cube into account.
            return m_cube.center() + pos * (m_cube.size() / 2);
        };

        const std::array<glm::vec3, 6> cube_directions{
            glm::vec3(-1.0f, 0.0f, 0.0f), // left
            glm::vec3(1.0f, 0.0f, 0.0f),  // right
            glm::vec3(0.0f, -1.0f, 0.0f), // front
            glm::vec3(0.0f, 1.0f, 0.0f),  // back
            glm::vec3(0.0f, 0.0f, 1.0f),  // top
            glm::vec3(0.0f, 0.0f, -1.0f)  // bottom
        };

        // The coordinates of the center of every face of the cube.
        const std::array<cube_face, 6> cube_face_centers{cube_face{"left", adjust_coordinates(cube_directions[0])},
                                                         cube_face{"right", adjust_coordinates(cube_directions[1])},
                                                         cube_face{"front", adjust_coordinates(cube_directions[2])},
                                                         cube_face{"back", adjust_coordinates(cube_directions[3])},
                                                         cube_face{"top", adjust_coordinates(cube_directions[4])},
                                                         cube_face{"bottom", adjust_coordinates(cube_directions[5])}};

        // The coordinates of every corner of the cube.
        std::array<cube_corner, 8> cube_corners = {
            cube_corner{"left back top", adjust_coordinates({-1.0f, 1.0f, 1.0f})},
            cube_corner{"right back top", adjust_coordinates({1.0f, 1.0f, 1.0f})},
            cube_corner{"left back botom", adjust_coordinates({-1.0f, 1.0f, -1.0f})},
            cube_corner{"right back bottom", adjust_coordinates({1.0f, 1.0f, -1.0f})},
            cube_corner{"left front top", adjust_coordinates({-1.0f, -1.0f, 1.0f})},
            cube_corner{"right front top", adjust_coordinates({1.0f, -1.0f, 1.0f})},
            cube_corner{"left front bottom", adjust_coordinates({-1.0f, -1.0f, -1.0f})},
            cube_corner{"right front bottom", adjust_coordinates({1.0f, -1.0f, -1.0f})}};

        using corner_on_face_index = std::array<std::size_t, 4>;

        // These indices specify which 4 points describe the corners on the given face.
        static constexpr std::array<corner_on_face_index, 6> CORNERS_ON_FACE_INDICES{
            corner_on_face_index{0, 2, 4, 6}, // left
            corner_on_face_index{1, 3, 5, 7}, // right
            corner_on_face_index{4, 5, 6, 6}, // front
            corner_on_face_index{0, 1, 2, 3}, // back
            corner_on_face_index{0, 1, 4, 5}, // top
            corner_on_face_index{2, 3, 6, 7}  // bottom
        };

        // The coordinates of the center of every edge of the cube.
        std::array<cube_edge, 12> cube_edges = {
            cube_edge{"left top", adjust_coordinates({-1.0f, 0.0f, 1.0f})},
            cube_edge{"left front", adjust_coordinates({-1.0f, 1.0f, 0.0f})},
            cube_edge{"left bottom", adjust_coordinates({-1.0f, 0.0f, -1.0f})},
            cube_edge{"left back", adjust_coordinates({-1.0f, -1.0f, 0.0f})},
            cube_edge{"right top", adjust_coordinates({1.0f, 0.0f, 1.0f})},
            cube_edge{"right front", adjust_coordinates({1.0f, 1.0f, 0.0f})},
            cube_edge{"right bottom", adjust_coordinates({1.0f, 0.0f, -1.0f})},
            cube_edge{"right back", adjust_coordinates({1.0f, -1.0f, 0.0f})},
            cube_edge{"middle bottom front", adjust_coordinates({0.0f, -1.0f, -1.0f})},
            cube_edge{"middle bottom back", adjust_coordinates({0.0f, 1.0f, -1.0f})},
            cube_edge{"middle top front", adjust_coordinates({0.0f, -1.0f, 1.0f})},
            cube_edge{"middle top back", adjust_coordinates({0.0f, 1.0f, 1.0f})}};

        using edge_on_face_index = std::array<std::size_t, 4>;

        // These indices specify which 4 edges are associated with a given face.
        static constexpr std::array<edge_on_face_index, 6> EDGE_ON_FACE_INDICES{
            edge_on_face_index{0, 1, 2, 3},   // left
            edge_on_face_index{4, 5, 6, 7},   // right
            edge_on_face_index{1, 5, 8, 11},  // front
            edge_on_face_index{3, 7, 9, 11},  // back
            edge_on_face_index{0, 4, 10, 11}, // top
            edge_on_face_index{2, 6, 8, 9}    // bottom
        };

        /// @brief Determine the intersection point between a ray and a plane.
        /// @note This function is not available in glm.
        /// @param plane_pos The position of the plane.
        /// @param plane_norm The normal vector of the plane.
        /// @param ray_pos Point a on the ray.
        /// @param ray_dir Point b on the ray.
        const auto ray_plane_intersection_point = [](const glm::vec3 plane_pos, const glm::vec3 plane_norm,
                                                     const glm::vec3 ray_pos, const glm::vec3 ray_dir) {
            return ray_pos - ray_dir * (glm::dot((ray_pos - plane_pos), plane_norm) / glm::dot(ray_dir, plane_norm));
        };

        /// @brief As soon as we determined which face is selected and we calculated the intersection point between the
        /// ray and the face plane, we need to determine the nearest corner in that face and the nearest edge on that
        /// face. In order to determine the nearest corner on a selected face for example, we would iterate through all
        /// corners on the selected face and calculate the distance between the intersection point and the corner's
        /// coordinates. The corner which is closest to the intersection point is the selected corner.
        /// However, we should not use ``glm::distance`` for this, because it performs a ``sqrt`` calculation on the
        /// vector coordinates. This is not necessary in this case, as we are not interested in the exact distance but
        /// rather in a value which allows us to determine the nearest corner. This means we can use the squared
        /// distance, which allows us to avoid the costly call of ``sqrt``.
        /// @param pos1 The first point.
        /// @param pos2 The second point.
        const auto square_of_distance = [](const std::array<glm::vec3, 2> points) {
            const auto diff = points[1] - points[0];
            return static_cast<float>(std::pow(glm::dot(diff, diff), 2));
        };

        float shortest_squared_distance{std::numeric_limits<float>::max()};
        float squared_distance{std::numeric_limits<float>::max()};

        std::size_t selected_face_index{0};

        // Loop though all faces of the cube and check for collision between ray and face plane.
        for (std::size_t i = 0; i < 6; i++) {

            // Check if the cube side is facing the camera: if the dot product of the two vectors is smaller than
            // zero, the corresponding angle is smaller than 90 degrees, so the side is facing the camera. Check the
            // references page for a detailed explanation of this formula.
            if (glm::dot(cube_directions[i], ray_dir) < 0.0f) {

                const auto intersection = ray_plane_intersection_point(std::get<1>(cube_face_centers[i]),
                                                                       cube_directions[i], ray_pos, ray_dir);

                squared_distance = square_of_distance({m_cube.center(), intersection});

                if (squared_distance < shortest_squared_distance) {
                    selected_face_index = i;
                    shortest_squared_distance = squared_distance;
                    m_intersection = intersection;
                    m_selected_face = cube_face_centers[i];
                }
            }
        }

        // Reset value to maximum for the search of the closest corner.
        shortest_squared_distance = std::numeric_limits<float>::max();

        // Loop through all corners of this face and check for the nearest one.
        for (const auto corner_index : CORNERS_ON_FACE_INDICES[selected_face_index]) {

            // TODO: use squared distance instead of glm::distance (no expensive sqrt call)
            squared_distance = square_of_distance({std::get<1>(cube_corners[corner_index]), m_intersection});

            if (squared_distance < shortest_squared_distance) {
                shortest_squared_distance = squared_distance;
                m_nearest_corner = cube_corners[corner_index];
            }
        }

        // Reset value to maximum for the search of the closest edge.
        shortest_squared_distance = std::numeric_limits<float>::max();

        // Iterate through all edges on this face and select the nearest one.
        for (const auto edge_index : EDGE_ON_FACE_INDICES[selected_face_index]) {

            squared_distance = square_of_distance({std::get<1>(cube_edges[edge_index]), m_intersection});

            if (squared_distance < shortest_squared_distance) {
                shortest_squared_distance = squared_distance;
                m_nearest_edge = cube_edges[edge_index];
            }
        }
    }

    RayCubeCollision(const RayCubeCollision &) = delete;

    RayCubeCollision(RayCubeCollision &&other) noexcept : m_cube{other.m_cube} {
        m_intersection = other.m_intersection;
        m_selected_face = std::move(other.m_selected_face);
        m_nearest_corner = std::move(other.m_nearest_corner);
        m_nearest_edge = std::move(other.m_nearest_edge);
    }

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

}; // namespace inexor::vulkan_renderer::world
