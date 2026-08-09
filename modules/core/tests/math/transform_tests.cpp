#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <woki/math/mat/mat4.hpp>
#include <woki/math/vec/vec4.hpp>
#include <woki/math/interop/op.hpp>
#include <woki/math/common/constants.hpp>
#include <woki/math/common/functions.hpp>
#include <woki/math/interop/transform.hpp>

using namespace woki::math;
using Catch::Approx;

TEST_CASE("radians and degrees conversion") {
    REQUIRE(radians(180.0f) == Approx(pi<float>));
    REQUIRE(radians(90.0f) == Approx(half_pi<float>));
    REQUIRE(degrees(pi<float>) == Approx(180.0f));
    REQUIRE(degrees(half_pi<float>) == Approx(90.0f));
}

TEST_CASE("lerp") {
    REQUIRE(lerp(0.0f, 10.0f, 0.0f) == 0.0f);
    REQUIRE(lerp(0.0f, 10.0f, 1.0f) == 10.0f);
    REQUIRE(lerp(0.0f, 10.0f, 0.5f) == 5.0f);
}

TEST_CASE("sign and abs") {
    REQUIRE(sign(5.0f) == 1.0f);
    REQUIRE(sign(-3.0f) == -1.0f);
    REQUIRE(sign(0.0f) == 0.0f);
    REQUIRE(abs(-5.0f) == 5.0f);
    REQUIRE(abs(5) == 5);
}

TEST_CASE("clamp") {
    REQUIRE(clamp(5.0f, 0.0f, 10.0f) == 5.0f);
    REQUIRE(clamp(-5.0f, 0.0f, 10.0f) == 0.0f);
    REQUIRE(clamp(15.0f, 0.0f, 10.0f) == 10.0f);
}

TEST_CASE("approx_equal") {
    REQUIRE(approx_equal(1.0f, 1.0f));
    REQUIRE(approx_equal(1.0f, 1.0f + epsilon<float>));
    REQUIRE_FALSE(approx_equal(1.0f, 2.0f));
}

TEST_CASE("mat4 translate") {
    auto m = translate(vec<3, float>(10.0f, 20.0f, 30.0f));
    vec<4, float> v(1.0f, 2.0f, 3.0f, 1.0f);
    auto result = m * v;
    REQUIRE(result[0] == Approx(11.0f));
    REQUIRE(result[1] == Approx(22.0f));
    REQUIRE(result[2] == Approx(33.0f));
    REQUIRE(result[3] == Approx(1.0f));
}

TEST_CASE("mat4 scale") {
    auto m = scale(vec<3, float>(2.0f, 3.0f, 4.0f));
    vec<4, float> v(1.0f, 1.0f, 1.0f, 1.0f);
    auto result = m * v;
    REQUIRE(result[0] == Approx(2.0f));
    REQUIRE(result[1] == Approx(3.0f));
    REQUIRE(result[2] == Approx(4.0f));
    REQUIRE(result[3] == Approx(1.0f));
}

TEST_CASE("mat4 rotate_x 90deg basis vector") {
    auto m = rotate_x(half_pi<float>);
    vec<4, float> v(0.0f, 1.0f, 0.0f, 1.0f);
    auto result = m * v;
    // Ground truth: rx(90) * [0,1,0,1] = [0, 6.123e-17, 1, 1]
    REQUIRE(result[0] == Approx(0.0f).margin(1e-6f));
    REQUIRE(result[1] == Approx(0.0f).margin(1e-6f));
    REQUIRE(result[2] == Approx(1.0f));
    REQUIRE(result[3] == Approx(1.0f));
}

TEST_CASE("mat4 rotate_y 90deg basis vector") {
    auto m = rotate_y(half_pi<float>);
    vec<4, float> v(0.0f, 0.0f, 1.0f, 1.0f);
    auto result = m * v;
    // Ground truth: ry(90) * [0,0,1,1] = [1, 0, 6.123e-17, 1]
    REQUIRE(result[0] == Approx(1.0f));
    REQUIRE(result[1] == Approx(0.0f).margin(1e-6f));
    REQUIRE(result[2] == Approx(0.0f).margin(1e-6f));
    REQUIRE(result[3] == Approx(1.0f));
}

TEST_CASE("mat4 rotate_z 90deg basis vector") {
    auto m = rotate_z(half_pi<float>);
    vec<4, float> v(1.0f, 0.0f, 0.0f, 1.0f);
    auto result = m * v;
    // Ground truth: rz(90) * [1,0,0,1] = [6.123e-17, 1, 0, 1]
    REQUIRE(result[0] == Approx(0.0f).margin(1e-6f));
    REQUIRE(result[1] == Approx(1.0f));
    REQUIRE(result[2] == Approx(0.0f).margin(1e-6f));
    REQUIRE(result[3] == Approx(1.0f));
}

TEST_CASE("mat4 perspective") {
    auto m = perspective(pi<float> / 2.0f, 16.0f / 9.0f, 0.1f, 100.0f);
    REQUIRE(m(0, 0) != 0.0f);
    REQUIRE(m(1, 1) != 0.0f);
    REQUIRE(m(2, 2) != 0.0f);
    REQUIRE(m(3, 2) == Approx(-1.0f));
}

TEST_CASE("mat4 perspective exact elements") {
    // Ground truth: fovy=60, aspect=16/9, z_near=0.1, z_far=100
    // [0.97427857, 0,          0,          0]
    // [0,          1.7320508,  0,          0]
    // [0,          0,         -1.002002,  -0.2002002]
    // [0,          0,         -1,          0]
    auto p = perspective(radians(60.0f), 16.0f / 9.0f, 0.1f, 100.0f);
    REQUIRE(p(0, 0) == Approx(0.97427857f).margin(1e-6f));
    REQUIRE(p(1, 1) == Approx(1.7320508f).margin(1e-6f));
    REQUIRE(p(2, 2) == Approx(-1.002002f).margin(1e-6f));
    REQUIRE(p(2, 3) == Approx(-0.2002002f).margin(1e-6f));
    REQUIRE(p(3, 2) == Approx(-1.0f));
}

TEST_CASE("mat4 ortho") {
    auto m = ortho(-1.0f, 1.0f, -1.0f, 1.0f, 0.1f, 100.0f);
    vec<4, float> v(1.0f, 1.0f, 0.1f, 1.0f);
    auto result = m * v;
    REQUIRE(result[0] == Approx(1.0f));
    REQUIRE(result[1] == Approx(1.0f));
}

TEST_CASE("mat4 lookAt produces valid view matrix") {
    vec<3, float> eye(0.0f, 0.0f, 5.0f);
    vec<3, float> center(0.0f, 0.0f, 0.0f);
    vec<3, float> up(0.0f, 1.0f, 0.0f);
    auto m = lookAt(eye, center, up);

    vec<4, float> world_center(center, 1.0f);
    auto view_center = m * world_center;

    // Center should map to the negative Z axis in view space
    REQUIRE(view_center[2] < 0.0f);
}

TEST_CASE("vec4 matrix multiplication identity") {
    mat<4, 4, float> m = mat<4, 4, float>::identity();
    vec<4, float> v(1.0f, 2.0f, 3.0f, 1.0f);
    auto result = m * v;
    REQUIRE(result[0] == 1.0f);
    REQUIRE(result[1] == 2.0f);
    REQUIRE(result[2] == 3.0f);
    REQUIRE(result[3] == 1.0f);
}

TEST_CASE("vec3 matrix multiplication order") {
    // Verify operator*(mat, vec) works with vec3 too
    auto tr = translate(vec<3, float>(1.0f, 2.0f, 3.0f));
    // Note: op.hpp only defines mat * vec when sizes match exactly.
    // For 4x4 mat * vec3, there isn't an overload. So we use vec4.
    vec<4, float> p4(0.0f, 0.0f, 0.0f, 1.0f);
    auto r4 = tr * p4;
    REQUIRE(r4[0] == Approx(1.0f));
    REQUIRE(r4[1] == Approx(2.0f));
    REQUIRE(r4[2] == Approx(3.0f));
}
