#include <glm/glm.hpp>
#include <immintrin.h>
#include <core/api.hpp>

namespace math
{
    APPLIB_API float dot(const glm::vec3 &a, const glm::vec3 &b)
    {
        __m128 a_x = _mm_set1_ps(a.x);
        __m128 a_y = _mm_set1_ps(a.y);
        __m128 a_z = _mm_set1_ps(a.z);

        __m128 b_x = _mm_set1_ps(b.x);
        __m128 b_y = _mm_set1_ps(b.y);
        __m128 b_z = _mm_set1_ps(b.z);

        __m128 result = _mm_add_ps(_mm_add_ps(_mm_mul_ps(a_x, b_x), _mm_mul_ps(a_y, b_y)), _mm_mul_ps(a_z, b_z));

        return _mm_cvtss_f32(result);
    }

    APPLIB_API float length2(const glm::vec3 &v)
    {
        __m128 v_x = _mm_set1_ps(v.x);
        __m128 v_y = _mm_set1_ps(v.y);
        __m128 v_z = _mm_set1_ps(v.z);

        __m128 result = _mm_add_ps(_mm_add_ps(_mm_mul_ps(v_x, v_x), _mm_mul_ps(v_y, v_y)), _mm_mul_ps(v_z, v_z));

        return _mm_cvtss_f32(result);
    }

    APPLIB_API float length(const glm::vec3 &v)
    {
        float lengthSquared = length2(v);
        return std::sqrt(lengthSquared);
    }

    APPLIB_API glm::vec3 normalize(const glm::vec3 &v)
    {
        __m128 v_vec = _mm_set_ps(0.0f, v.z, v.y, v.x);
        __m128 mul = _mm_mul_ps(v_vec, v_vec);
        __m128 hadd = _mm_hadd_ps(mul, mul);
        hadd = _mm_hadd_ps(hadd, hadd);
        float length = std::sqrt(_mm_cvtss_f32(hadd));

        if (length == 0.0f) return v;

        // Деление компонентов на длину
        __m128 length_vec = _mm_set1_ps(length);
        __m128 normalized_vec = _mm_div_ps(v_vec, length_vec);

        return glm::vec3(_mm_cvtss_f32(normalized_vec),
                         _mm_cvtss_f32(_mm_shuffle_ps(normalized_vec, normalized_vec, _MM_SHUFFLE(1, 1, 1, 1))),
                         _mm_cvtss_f32(_mm_shuffle_ps(normalized_vec, normalized_vec, _MM_SHUFFLE(2, 2, 2, 2))));
    }
} // namespace math