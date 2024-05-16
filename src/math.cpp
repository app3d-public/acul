#include <core/api.hpp>
#include <core/std/basic_types.hpp>
#include <glm/glm.hpp>


namespace math
{
    APPLIB_API glm::vec2 screenToNdc(const glm::vec2 &screen, Point2D extent)
    {
        float xNdc = (screen.x / extent.x) * 2.0f - 1;
        float yNdc = 1.0f - (screen.y / extent.y) * 2.0f;
        return {xNdc, yNdc};
    }

    APPLIB_API std::pair<glm::vec3, glm::vec3> createRay(const glm::vec2 &ndcPos, const glm::mat4 &viewInverse,
                                                         const glm::mat4 &projection)
    {
        glm::mat4 invProjection = glm::inverse(projection);
        glm::vec4 startView = invProjection * glm::vec4{ndcPos, -1.0f, 1.0f};
        startView /= startView.w;
        glm::vec4 endView = invProjection * glm::vec4{ndcPos, 1.0f, 1.0f};
        endView /= endView.w;

        glm::vec4 startWorld = viewInverse * startView;
        startWorld /= startWorld.w;
        glm::vec4 endWorld = viewInverse * endView;
        endWorld /= endWorld.w;

        glm::vec3 rayDirection = glm::normalize(glm::vec3(endWorld - startWorld));
        return {glm::vec3(startWorld), rayDirection};
    }

    APPLIB_API bool intersectRayWithPlane(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection,
                                          const glm::vec3 &planeNormal, f32 planeDistance, glm::vec3 &intersection)
    {
        f32 denom = glm::dot(rayDirection, planeNormal);
        if (fabs(denom) > std::numeric_limits<f32>::epsilon())
        {
            f32 t = (planeDistance - glm::dot(rayOrigin, planeNormal)) / denom;
            intersection = rayOrigin + t * rayDirection;
            return t >= 0;
        }
        return false;
    }

    APPLIB_API glm::vec3 ndcToWorld(const glm::vec2 &ndc, const glm::mat4 &projection, const glm::mat4 &viewInverse)
    {
        glm::vec3 planeNormal = glm::normalize(glm::vec3{-viewInverse[2]});
        auto [rayOrigin, rayDirection] = createRay({ndc.x, -ndc.y}, viewInverse, projection);
        glm::vec3 intersection;
        if (intersectRayWithPlane(rayOrigin, rayDirection, planeNormal, 0.0f, intersection)) return -intersection;
        return {0.0f, 0.0f, 0.0f};
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

    APPLIB_API bool operator==(const glm::vec3 &a, const glm::vec3 &b)
    {
        return a.x == b.x && a.y == b.y && a.z == b.z;
    }

    APPLIB_API bool operator<(const glm::vec2 &a, const glm::vec2 &b)
    {
        if (a.x != b.x) return a.x < b.x;
        if (a.y != b.y) return a.y < b.y;
        return false;
    }

    APPLIB_API bool operator==(const glm::vec2 &a, const glm::vec2 &b) { return a.x == b.x && a.y == b.y; }
} // namespace glm