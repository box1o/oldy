#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <woki/math/mat/mat4.hpp>
#include <woki/math/vec/vec3.hpp>
#include <woki/math/quat/quat.hpp>
#include <woki/math/common/constants.hpp>

using namespace woki::math;
using Catch::Approx;

TEST_CASE("quat default construction") {
    quat<float> q;
    REQUIRE(q.x == 0.0f);
    REQUIRE(q.y == 0.0f);
    REQUIRE(q.z == 0.0f);
    REQUIRE(q.w == 0.0f);
}

TEST_CASE("quat component construction") {
    quat<float> q(1.0f, 2.0f, 3.0f, 4.0f);
    REQUIRE(q.x == 1.0f);
    REQUIRE(q.y == 2.0f);
    REQUIRE(q.z == 3.0f);
    REQUIRE(q.w == 4.0f);
}

TEST_CASE("quat from axis-angle") {
    quat<float> q(vec<3, float>(0.0f, 0.0f, 1.0f), half_pi<float>);
    REQUIRE(q.length() == Approx(1.0f));
    REQUIRE(q.w == Approx(std::cos(half_pi<float> / 2.0f)));
}

TEST_CASE("quat from euler angles") {
    quat<float> q(vec<3, float>(0.0f, 0.0f, half_pi<float>));
    REQUIRE(q.length() == Approx(1.0f));
}

TEST_CASE("quat normalize") {
    quat<float> q(1.0f, 2.0f, 3.0f, 4.0f);
    auto n = q.normalized();
    REQUIRE(n.length() == Approx(1.0f));
}

TEST_CASE("quat conjugate") {
    quat<float> q(1.0f, 2.0f, 3.0f, 4.0f);
    auto c = q.conjugate();
    REQUIRE(c.x == -1.0f);
    REQUIRE(c.y == -2.0f);
    REQUIRE(c.z == -3.0f);
    REQUIRE(c.w == 4.0f);
}

TEST_CASE("quat inverse") {
    quat<float> q = quat<float>(vec<3, float>(0.0f, 1.0f, 0.0f), half_pi<float>).normalized();
    auto inv = q.inverse();
    auto id = q * inv;
    REQUIRE(id.w == Approx(1.0f));
    REQUIRE(id.x == Approx(0.0f).margin(1e-5f));
    REQUIRE(id.y == Approx(0.0f).margin(1e-5f));
    REQUIRE(id.z == Approx(0.0f).margin(1e-5f));
}

TEST_CASE("quat multiplication identity") {
    quat<float> q = quat<float>(vec<3, float>(0.0f, 0.0f, 1.0f), half_pi<float>).normalized();
    quat<float> id(0.0f, 0.0f, 0.0f, 1.0f);
    auto r = q * id;
    REQUIRE(r == q);
}

TEST_CASE("quat rotate vector") {
    quat<float> q(vec<3, float>(0.0f, 0.0f, 1.0f), half_pi<float>);
    q.normalize();
    vec<3, float> v(1.0f, 0.0f, 0.0f);
    auto r = q.rotate(v);
    REQUIRE(r.x == Approx(0.0f).margin(1e-4f));
    REQUIRE(r.y == Approx(1.0f).margin(1e-4f));
    REQUIRE(r.z == Approx(0.0f).margin(1e-4f));
}

TEST_CASE("quat toMat4 exact values") {
    // Ground truth: quat(axis=[1,0,0], angle=90deg) = [0.7071, 0, 0, 0.7071]
    // toMat4 gives:
    // [[1,  0,       0,      0],
    //  [0, ~6e-8,  -1,      0],
    //  [0,  1,      ~6e-8,  0],
    //  [0,  0,       0,      1]]
    quat<float> q(vec<3, float>(1.0f, 0.0f, 0.0f), half_pi<float>);
    q.normalize();
    auto m = q.toMat4();
    REQUIRE(m(0, 0) == Approx(1.0f));
    REQUIRE(m(1, 1) == Approx(0.0f).margin(1e-6f));
    REQUIRE(m(1, 2) == Approx(-1.0f));
    REQUIRE(m(2, 1) == Approx(1.0f));
    REQUIRE(m(2, 2) == Approx(0.0f).margin(1e-6f));
}

TEST_CASE("quat fromMat4 roundtrip") {
    quat<float> q(vec<3, float>(1.0f, 0.0f, 0.0f), half_pi<float>);
    q.normalize();
    auto m = q.toMat4();
    auto q2 = quat<float>::fromMat4(m);
    // q and -q represent the same rotation
    bool same = (q2 == q);
    bool opposite = (q2 == -q);
    REQUIRE((same || opposite));
}

TEST_CASE("quat slerp") {
    quat<float> a(vec<3, float>(0.0f, 0.0f, 1.0f), 0.0f);
    a.normalize();
    quat<float> b(vec<3, float>(0.0f, 0.0f, 1.0f), half_pi<float>);
    b.normalize();

    auto mid = slerp(a, b, 0.5f);
    REQUIRE(mid.length() == Approx(1.0f));

    auto start = slerp(a, b, 0.0f);
    auto end = slerp(a, b, 1.0f);
    REQUIRE(start == a);
    REQUIRE(end == b);
}

TEST_CASE("quat slerp midpoint exact") {
    // Ground truth: slerp(id, z-rot-90, 0.5) = [0, 0, 0.3826835, 0.9238795]
    quat<float> a(0.0f, 0.0f, 0.0f, 1.0f);
    quat<float> b(vec<3, float>(0.0f, 0.0f, 1.0f), half_pi<float>);
    b.normalize();

    auto mid = slerp(a, b, 0.5f);
    REQUIRE(mid.x == Approx(0.0f).margin(1e-6f));
    REQUIRE(mid.y == Approx(0.0f).margin(1e-6f));
    REQUIRE(mid.z == Approx(0.3826835f).margin(1e-6f));
    REQUIRE(mid.w == Approx(0.9238795f).margin(1e-6f));
    REQUIRE(mid.length() == Approx(1.0f).margin(1e-6f));
}

TEST_CASE("quat comparison") {
    quat<float> a(1.0f, 2.0f, 3.0f, 4.0f);
    quat<float> b(1.0f, 2.0f, 3.0f, 4.0f);
    quat<float> c(1.0f, 2.0f, 3.0f, 5.0f);
    REQUIRE(a == b);
    REQUIRE(a != c);
}

TEST_CASE("quat unary minus") {
    quat<float> q(1.0f, -2.0f, 3.0f, -4.0f);
    auto n = -q;
    REQUIRE(n.x == -1.0f);
    REQUIRE(n.y == 2.0f);
    REQUIRE(n.z == -3.0f);
    REQUIRE(n.w == 4.0f);
}
