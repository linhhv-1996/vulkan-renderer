#pragma once

#include "inexor/vulkan-renderer/world/indentation.hpp"

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

// forward declaration
namespace inexor::vulkan_renderer::world {
class Cube;
} // namespace inexor::vulkan_renderer::world

// forward declaration
namespace inexor::vulkan_renderer::io {
class ByteStream;
template <std::size_t version>
[[nodiscard]] std::shared_ptr<world::Cube> deserialize_octree_impl(const ByteStream &stream);

} // namespace inexor::vulkan_renderer::io

void swap(inexor::vulkan_renderer::world::Cube &lhs, inexor::vulkan_renderer::world::Cube &rhs) noexcept;

namespace inexor::vulkan_renderer::world {

/// std::vector<Polygon> can probably replaced with an array.
using Polygon = std::array<glm::vec3, 3>;

using PolygonCache = std::shared_ptr<std::vector<Polygon>>;

class Cube : public std::enable_shared_from_this<Cube> {
    friend void ::swap(Cube &lhs, Cube &rhs) noexcept;
    template <std::size_t version>
    friend std::shared_ptr<world::Cube> io::deserialize_octree_impl(const io::ByteStream &stream);

public:
    /// Maximum of sub cubes (childs).
    static constexpr std::size_t SUB_CUBES = 8;
    /// Cube edges.
    static constexpr std::size_t EDGES = 12;
    /// Cube Type.
    enum class Type { EMPTY = 0b00U, SOLID = 0b01U, NORMAL = 0b10U, OCTANT = 0b11U };

    /// IDs of the children and edges which will be swapped to receive the rotation.
    /// To achieve a 90 degree rotation the 0th index have to be swapped with the 1st and the 1st with the 2nd, etc.
    struct RotationAxis {
        using ChildType = std::array<std::array<std::size_t, 4>, 2>;
        using EdgeType = std::array<std::array<std::size_t, 4>, 3>;
        using Type = std::pair<ChildType, EdgeType>;
        /// IDs of the children / edges which will be swapped to receive the rotation around X axis.
        static constexpr Type X{{{{0, 1, 3, 2}, {4, 5, 7, 6}}}, {{{2, 4, 11, 1}, {5, 7, 8, 10}, {0, 9, 6, 3}}}};
        /// IDs of the children / edges which will be swapped to receive the rotation around Y axis.
        static constexpr Type Y{{{{0, 4, 5, 1}, {2, 6, 7, 3}}}, {{{0, 5, 9, 2}, {3, 8, 6, 11}, {1, 10, 7, 4}}}};
        /// IDs of the children / edges which will be swapped to receive the rotation around Z axis.
        static constexpr Type Z{{{{0, 2, 6, 4}, {1, 3, 7, 5}}}, {{{1, 3, 10, 0}, {4, 6, 7, 9}, {2, 11, 8, 5}}}};
    };

private:
    Type m_type{Type::SOLID};
    float m_size{32};
    glm::vec3 m_position{0.0f, 0.0f, 0.0f};

    /// Root cube points to itself.
    std::weak_ptr<Cube> m_parent{weak_from_this()};

    /// Indentations, should only be used if it is a geometry cube.
    std::array<Indentation, Cube::EDGES> m_indentations{};
    std::array<std::shared_ptr<Cube>, Cube::SUB_CUBES> m_childs{};

    /// Only geometry cube (Type::SOLID and Type::Normal) have a polygon cache.
    mutable PolygonCache m_polygon_cache{nullptr};
    mutable bool m_polygon_cache_valid{false};

    /// Removes all childs recursive.
    void remove_childs();

    /// Get the root to this cube.
    [[nodiscard]] std::weak_ptr<Cube> root() const noexcept;
    /// Get the vertices of this cube. Use only on geometry cubes.
    [[nodiscard]] std::array<glm::vec3, 8> vertices() const noexcept;

    /// Optimized implementations of 90°, 180° and 270° rotations.
    template <int Rotations>
    void rotate(const RotationAxis::Type &axis);

public:
    Cube() = default;
    explicit Cube(Type type);
    Cube(Type type, float size, const glm::vec3 &position);
    Cube(std::weak_ptr<Cube> parent, Type type, float size, const glm::vec3 &position);
    Cube(const Cube &rhs);
    Cube(Cube &&rhs) noexcept;
    ~Cube() = default;
    Cube &operator=(Cube rhs);
    /// Get child.
    std::shared_ptr<Cube> operator[](std::size_t idx);
    /// Get child.
    const std::shared_ptr<const Cube> operator[](std::size_t idx) const;

    /// Is the current cube root.
    [[nodiscard]] bool is_root() const noexcept;
    /// Is the current cube a leaf.
    [[nodiscard]] bool is_leaf() const noexcept;
    /// At which child level this cube is.
    /// root cube = 0
    [[nodiscard]] std::size_t grid_level() const noexcept;
    /// Count the number of Type::SOLID and Type::NORMAL cubes.
    [[nodiscard]] std::size_t count_geometry_cubes() const noexcept;

    [[nodiscard]] glm::vec3 center() const noexcept {
        return m_position + 0.5f * m_size;
    }

    [[nodiscard]] const glm::vec3 &position() const noexcept {
        return m_position;
    }

    [[nodiscard]] std::array<glm::vec3, 2> bounding_box() const {
        return {m_position, {m_position.x + m_size, m_position.y + m_size, m_position.z + m_size}};
    }

    [[nodiscard]] float bounding_box_radius() const {
        return static_cast<float>(std::sqrt(3) * m_size) / 2.0f;
    }

    [[nodiscard]] float size() const noexcept {
        return m_size;
    }

    [[nodiscard]] float squared_distance(const glm::vec3 pos) const {
        const auto diff = center() - pos;
        return static_cast<float>(std::pow(glm::dot(diff, diff), 2));
    }

    /// Set a new type.
    void set_type(Type new_type);
    /// Get type.
    [[nodiscard]] Type type() const noexcept;

    /// Get childs.
    [[nodiscard]] const std::array<std::shared_ptr<Cube>, Cube::SUB_CUBES> &childs() const noexcept;
    /// Get indentations.
    [[nodiscard]] std::array<Indentation, Cube::EDGES> indentations() const noexcept;

    /// Set an indent by the edge id.
    void set_indent(std::uint8_t edge_id, Indentation indentation);
    /// Indent a specific edge by steps.
    /// @param positive_direction Indent in  positive axis direction.
    void indent(std::uint8_t edge_id, bool positive_direction, std::uint8_t steps);

    /// Rotate the cube 90° clockwise around the given axis. Repeats with the given rotations.
    /// @param axis Only one index should be one.
    /// @param rotations Value does not need to be adjusted beforehand. (e.g. mod 4)
    void rotate(const RotationAxis::Type &axis, int rotations);

    /// TODO: in special cases some polygons have no surface, if completely surrounded by others
    /// \warning Will update the cache even if it is considered as valid.
    void update_polygon_cache() const;
    /// Invalidate polygon cache.
    void invalidate_polygon_cache() const;
    /// Recursive way to collect all the caches.
    /// @param update_invalid If true it will update invalid polygon caches.
    [[nodiscard]] std::vector<PolygonCache> polygons(bool update_invalid = false) const;
};

} // namespace inexor::vulkan_renderer::world
