#include <acul/api.hpp>
#include <acul/pair.hpp>
#include <acul/scalars.hpp>
#include <glm/glm.hpp>

namespace acul
{
    namespace math
    {
        APPLIB_API std::pair<glm::vec3, glm::vec3> create_ray(const glm::vec2 &ndc_pos, const glm::mat4 &view_inverse,
                                                              const glm::mat4 &projection)
        {
            glm::mat4 inv_projection = glm::inverse(projection);
            glm::vec4 start_view = inv_projection * glm::vec4{ndc_pos, -1.0f, 1.0f};
            start_view /= start_view.w;
            glm::vec4 endView = inv_projection * glm::vec4{ndc_pos, 1.0f, 1.0f};
            endView /= endView.w;

            glm::vec4 startWorld = view_inverse * start_view;
            startWorld /= startWorld.w;
            glm::vec4 endWorld = view_inverse * endView;
            endWorld /= endWorld.w;

            glm::vec3 rayDirection = glm::normalize(glm::vec3(endWorld - startWorld));
            return {glm::vec3(startWorld), rayDirection};
        }
    } // namespace math
} // namespace acul