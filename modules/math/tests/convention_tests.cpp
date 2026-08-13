#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <woki/math.hpp>

using Catch::Approx;
using namespace woki::math;

TEST_CASE("vectors and matrices use column-vector multiplication") {
    const auto transform = translate(vec<3, float>(2.0F, 3.0F, 4.0F)) * scale(vec<3, float>(5.0F, 6.0F, 7.0F));
    const auto point = transform * vec<4, float>(1.0F, 1.0F, 1.0F, 1.0F);
    REQUIRE(point.x == Approx(7.0F));
    REQUIRE(point.y == Approx(9.0F));
    REQUIRE(point.z == Approx(11.0F));
    REQUIRE(point.w == Approx(1.0F));

    const mat<2, 3, int> left(layout::rowm, 1, 2, 3, 4, 5, 6);
    const mat<3, 2, int> right(layout::rowm, 7, 8, 9, 10, 11, 12);
    const auto product = left * right;
    REQUIRE(product(0, 0) == 58);
    REQUIRE(product(0, 1) == 64);
    REQUIRE(product(1, 0) == 139);
    REQUIRE(product(1, 1) == 154);
}

TEST_CASE("quaternion composition agrees with matrix composition") {
    const quat<float> x(vec<3, float>(1.0F, 0.0F, 0.0F), half_pi<float>);
    const quat<float> z(vec<3, float>(0.0F, 0.0F, 1.0F), half_pi<float>);
    const vec<3, float> input(0.0F, 1.0F, 0.0F);
    const auto quaternion_result = (z * x).rotate(input);
    const auto matrix_result = (z.toMat4() * x.toMat4()) * vec<4, float>(input, 0.0F);
    REQUIRE(quaternion_result.x == Approx(matrix_result.x).margin(1e-5F));
    REQUIRE(quaternion_result.y == Approx(matrix_result.y).margin(1e-5F));
    REQUIRE(quaternion_result.z == Approx(matrix_result.z).margin(1e-5F));

    const auto recovered = quat<float>::fromMat4((z * x).toMat4());
    REQUIRE(recovered.rotate(input).x == Approx(quaternion_result.x).margin(1e-5F));
    REQUIRE(recovered.rotate(input).y == Approx(quaternion_result.y).margin(1e-5F));
    REQUIRE(recovered.rotate(input).z == Approx(quaternion_result.z).margin(1e-5F));
}

TEST_CASE("projection is right-handed with OpenGL minus-one-to-one depth") {
    constexpr float near_plane = 0.5F;
    constexpr float far_plane = 20.0F;
    const auto projection = perspective(half_pi<float>, 1.0F, near_plane, far_plane);
    const auto identity = mat<4, 4, float>::identity();

    const auto near_ndc = project_ndc(vec<3, float>(0.0F, 0.0F, -near_plane), identity, projection);
    const auto far_ndc = project_ndc(vec<3, float>(0.0F, 0.0F, -far_plane), identity, projection);
    const auto right = project_ndc(vec<3, float>(1.0F, 0.0F, -1.0F), identity, projection);
    REQUIRE(near_ndc.z == Approx(-1.0F).margin(1e-5F));
    REQUIRE(far_ndc.z == Approx(1.0F).margin(1e-5F));
    REQUIRE(right.x == Approx(1.0F).margin(1e-5F));
    REQUIRE(projection(3, 2) == -1.0F);
}

TEST_CASE("orthographic projection and unprojection preserve boundary points") {
    const auto projection = ortho(-2.0F, 6.0F, -4.0F, 4.0F, 1.0F, 9.0F);
    const auto identity = mat<4, 4, float>::identity();
    const vec<3, float> point(6.0F, -4.0F, -9.0F);
    const auto ndc = project_ndc(point, identity, projection);
    REQUIRE(ndc.x == Approx(1.0F));
    REQUIRE(ndc.y == Approx(-1.0F));
    REQUIRE(ndc.z == Approx(1.0F));
    const auto restored = unproject_ndc(ndc, identity, projection);
    REQUIRE(restored.x == Approx(point.x).margin(1e-5F));
    REQUIRE(restored.y == Approx(point.y).margin(1e-5F));
    REQUIRE(restored.z == Approx(point.z).margin(1e-5F));
}
