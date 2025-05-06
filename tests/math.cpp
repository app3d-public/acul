#include <acul/math.hpp>
#include <cassert>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <limits>

void test_screen_to_ndc()
{
    using namespace acul::math;

    acul::point2D<i32> extent{800, 600};

    glm::vec2 ndc = screen_to_ndc({400.0f, 300.0f}, extent);
    assert(fabs(ndc.x - 0.0f) < 0.0001f);
    assert(fabs(ndc.y - 0.0f) < 0.0001f);

    ndc = screen_to_ndc({0.0f, 0.0f}, extent);
    assert(fabs(ndc.x + 1.0f) < 0.0001f);
    assert(fabs(ndc.y - 1.0f) < 0.0001f);

    ndc = screen_to_ndc({800.0f, 600.0f}, extent);
    assert(fabs(ndc.x - 1.0f) < 0.0001f);
    assert(fabs(ndc.y + 1.0f) < 0.0001f);
}

void test_intersect_ray_with_plane()
{
    using namespace acul::math;

    glm::vec3 ray_origin(0.0f, 0.0f, 0.0f);
    glm::vec3 ray_direction(0.0f, 0.0f, -1.0f);
    glm::vec3 plane_normal(0.0f, 0.0f, 1.0f);
    glm::vec3 intersection;

    bool hit = intersect_ray_with_plane(ray_origin, ray_direction, plane_normal, -5.0f, intersection);
    assert(hit);
    assert(fabs(intersection.z + 5.0f) < 0.0001f);

    ray_direction = glm::vec3(1.0f, 0.0f, 0.0f);
    hit = intersect_ray_with_plane(ray_origin, ray_direction, plane_normal, 5.0f, intersection);
    assert(!hit);
}

void test_round10()
{
    using namespace acul::math;

    assert(round10(0.0f) == 0.0f);
    assert(round10(47.0f) == 100.0f);
    assert(round10(1500.0f) == 1000.0f);
    assert(round10(0.05f) == 0.1f);
    assert(round10(-25.0f) == -10.0f);
}

void test_get_type_min_max()
{
    using namespace acul::math;

    assert(get_type_min<int>() == std::numeric_limits<int>::lowest());
    assert(get_type_max<int>() == std::numeric_limits<int>::max());

    glm::vec2 vmin = get_type_min<glm::vec2>();
    glm::vec2 vmax = get_type_max<glm::vec2>();
    assert(vmin.x == std::numeric_limits<float>::lowest());
    assert(vmax.x == std::numeric_limits<float>::max());

    glm::mat4 mmin = get_type_min<glm::mat4>();
    glm::mat4 mmax = get_type_max<glm::mat4>();
    assert(mmin[0][0] == std::numeric_limits<float>::lowest());
    assert(mmax[0][0] == std::numeric_limits<float>::max());
}

void test_min_max_struct()
{
    using namespace acul::math;

    acul::math::min_max<int> mm;
    assert(mm.min == std::numeric_limits<int>::max());
    assert(mm.max == std::numeric_limits<int>::lowest());
}

void test_glm_operators()
{
    using namespace glm;

    vec2 a(1.0f, 2.0f);
    vec2 b(1.0f, 3.0f);
    vec2 c(1.0f, 2.0f);

    assert(a < b);
    assert(!(b < a));
    assert(a == c);

    vec3 d(1.0f, 2.0f, 3.0f);
    vec3 e(1.0f, 2.0f, 4.0f);
    vec3 f(1.0f, 2.0f, 3.0f);

    assert(d < e);
    assert(!(e < d));
    assert(d == f);
}

void test_glm_hash()
{
    using namespace std;
    using namespace glm;

    hash<ivec2> h2;
    hash<ivec3> h3;
    hash<vec2> hf2;
    hash<vec3> hf3;

    ivec2 a{1, 2};
    ivec2 b{1, 3};
    assert(h2(a) != h2(b));

    ivec3 c{1, 2, 3};
    ivec3 d{1, 2, 4};
    assert(h3(c) != h3(d));

    vec2 e{1.0f, 2.0f};
    vec2 f{1.0f, 3.0f};
    assert(hf2(e) != hf2(f));

    vec3 g{1.0f, 2.0f, 3.0f};
    vec3 h{1.0f, 2.0f, 4.0f};
    assert(hf3(g) != hf3(h));
}

void test_create_ray_and_ndc_to_world()
{
    using namespace acul::math;

    glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    glm::mat4 view = glm::lookAt(glm::vec3(0, 0, 5), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
    glm::mat4 view_inv = glm::inverse(view);

    glm::vec2 ndc(0.0f, 0.0f);

    auto [origin, dir] = create_ray(ndc, view_inv, proj);
    assert(glm::length(dir) > 0.9f);
    assert(origin.z > 4.9f && origin.z < 5.1f);

    glm::vec3 world = ndc_to_world(ndc, proj, view_inv);
    assert(fabs(world.z) < 0.0001f);
}

void test_math()
{
    test_screen_to_ndc();
    test_intersect_ray_with_plane();
    test_round10();
    test_get_type_min_max();
    test_min_max_struct();
    test_glm_operators();
    test_glm_hash();
    test_create_ray_and_ndc_to_world();
}