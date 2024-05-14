#include <core/std/basic_types.hpp>
#include <glm/glm.hpp>
#include <core/api.hpp>

namespace math
{
    APPLIB_API glm::vec2 screenToNdc(const glm::vec2 &screen, Point2D extent)
    {
        float xNdc = (screen.x / extent.x) * 2.0f - 1;
        float yNdc = 1.0f - (screen.y / extent.y) * 2.0f;
        return {xNdc, yNdc};
    }

    APPLIB_API glm::vec3 ndcToWorld(const glm::vec3 &ndc, const glm::mat4 &projection, const glm::mat4 &view)
    {
        glm::mat4 vpi = glm::inverse(projection * view);
        glm::vec4 ndcAffine{ndc.x, ndc.y, 0.0f, 1.0f};
        glm::vec4 worldAffine = ndcAffine * vpi;
        return glm::vec3{worldAffine} * ndc.z;
    }
} // namespace math

namespace glm
{
    APPLIB_API bool operator<(const glm::vec3 &a, const glm::vec3 &b)
    {
        if (a.x != b.x) return a.x < b.x;
        if (a.y != b.y) return a.y < b.y;
        return a.z < b.z;
    }

    APPLIB_API bool operator==(const glm::vec3 &a, const glm::vec3 &b) { return a.x == b.x && a.y == b.y && a.z == b.z; }

    APPLIB_API bool operator<(const glm::vec2 &a, const glm::vec2 &b)
    {
        if (a.x != b.x) return a.x < b.x;
        if (a.y != b.y) return a.y < b.y;
        return false;
    }

    APPLIB_API bool operator==(const glm::vec2 &a, const glm::vec2 &b) { return a.x == b.x && a.y == b.y; }
} // namespace glm