#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <woki/math/vec/vec2.hpp>
#include <woki/math/vec/vec3.hpp>
#include <woki/math/vec/vec4.hpp>
#include <woki/math/vec/functions.hpp>
#include <woki/math/common/constants.hpp>
#include <woki/math/common/functions.hpp>
#include <woki/math/interop/transform.hpp>
#include <woki/math/interop/projection.hpp>

using namespace woki::math;
using Catch::Approx;

TEST_CASE("vec2 default construction") {
    vec<2, float> v;
    REQUIRE(v.x == 0.0f);
    REQUIRE(v.y == 0.0f);
}

TEST_CASE("vec2 scalar construction") {
    vec<2, float> v(5.0f);
    REQUIRE(v.x == 5.0f);
    REQUIRE(v.y == 5.0f);
}

TEST_CASE("vec2 component construction") {
    vec<2, float> v(1.0f, 2.0f);
    REQUIRE(v.x == 1.0f);
    REQUIRE(v.y == 2.0f);
}

TEST_CASE("vec2 union access") {
    vec<2, float> v(1.0f, 2.0f);
    REQUIRE(v.r == 1.0f);
    REQUIRE(v.g == 2.0f);
}

TEST_CASE("vec2 subscript access") {
    vec<2, float> v(1.0f, 2.0f);
    REQUIRE(v[0] == 1.0f);
    REQUIRE(v[1] == 2.0f);
    v[0] = 3.0f;
    REQUIRE(v.x == 3.0f);
}

TEST_CASE("vec2 arithmetic") {
    vec<2, float> a(1.0f, 2.0f);
    vec<2, float> b(3.0f, 4.0f);

    auto c = a + b;
    REQUIRE(c.x == 4.0f);
    REQUIRE(c.y == 6.0f);

    auto d = b - a;
    REQUIRE(d.x == 2.0f);
    REQUIRE(d.y == 2.0f);

    auto e = a * b;
    REQUIRE(e.x == 3.0f);
    REQUIRE(e.y == 8.0f);

    auto f = a * 2.0f;
    REQUIRE(f.x == 2.0f);
    REQUIRE(f.y == 4.0f);

    auto g = 3.0f * a;
    REQUIRE(g.x == 3.0f);
    REQUIRE(g.y == 6.0f);

    auto h = b / a;
    REQUIRE(h.x == 3.0f);
    REQUIRE(h.y == 2.0f);

    auto i = b / 2.0f;
    REQUIRE(i.x == 1.5f);
    REQUIRE(i.y == 2.0f);
}

TEST_CASE("vec2 compound assignment") {
    vec<2, float> v(1.0f, 2.0f);
    v += vec<2, float>(1.0f, 1.0f);
    REQUIRE(v.x == 2.0f);
    REQUIRE(v.y == 3.0f);

    v -= vec<2, float>(1.0f, 1.0f);
    REQUIRE(v.x == 1.0f);
    REQUIRE(v.y == 2.0f);

    v *= 2.0f;
    REQUIRE(v.x == 2.0f);
    REQUIRE(v.y == 4.0f);

    v /= 2.0f;
    REQUIRE(v.x == 1.0f);
    REQUIRE(v.y == 2.0f);
}

TEST_CASE("vec2 unary minus") {
    vec<2, float> v(1.0f, -2.0f);
    auto n = -v;
    REQUIRE(n.x == -1.0f);
    REQUIRE(n.y == 2.0f);
}

TEST_CASE("vec2 comparison") {
    vec<2, float> a(1.0f, 2.0f);
    vec<2, float> b(1.0f, 2.0f);
    vec<2, float> c(1.0f, 3.0f);

    REQUIRE(a == b);
    REQUIRE(a != c);
}

TEST_CASE("vec2 length and normalize") {
    vec<2, float> v(3.0f, 4.0f);
    REQUIRE(v.length() == Approx(5.0f));
    REQUIRE(v.length_squared() == 25.0f);

    auto n = v.normalized();
    REQUIRE(n.length() == Approx(1.0f));
    REQUIRE(n.x == Approx(0.6f));
    REQUIRE(n.y == Approx(0.8f));

    vec<2, float> zero(0.0f, 0.0f);
    auto nz = zero.normalized();
    REQUIRE(nz.x == 0.0f);
    REQUIRE(nz.y == 0.0f);
}

TEST_CASE("vec2 dot and cross") {
    vec<2, float> a(1.0f, 0.0f);
    vec<2, float> b(0.0f, 1.0f);

    REQUIRE(a.dot(b) == 0.0f);
    REQUIRE(a.cross(b) == 1.0f);
}

TEST_CASE("vec3 default construction") {
    vec<3, float> v;
    REQUIRE(v.x == 0.0f);
    REQUIRE(v.y == 0.0f);
    REQUIRE(v.z == 0.0f);
}

TEST_CASE("vec3 component construction") {
    vec<3, float> v(1.0f, 2.0f, 3.0f);
    REQUIRE(v.x == 1.0f);
    REQUIRE(v.y == 2.0f);
    REQUIRE(v.z == 3.0f);
}

TEST_CASE("vec3 from vec2") {
    vec<2, float> xy(1.0f, 2.0f);
    vec<3, float> v(xy, 3.0f);
    REQUIRE(v.x == 1.0f);
    REQUIRE(v.y == 2.0f);
    REQUIRE(v.z == 3.0f);
}

TEST_CASE("vec3 cross product") {
    vec<3, float> a(1.0f, 0.0f, 0.0f);
    vec<3, float> b(0.0f, 1.0f, 0.0f);
    auto c = a.cross(b);
    REQUIRE(c.x == 0.0f);
    REQUIRE(c.y == 0.0f);
    REQUIRE(c.z == 1.0f);
}

TEST_CASE("vec3 accessors") {
    vec<3, float> v(1.0f, 2.0f, 3.0f);
    auto xy = v.xy();
    REQUIRE(xy.x == 1.0f);
    REQUIRE(xy.y == 2.0f);

    auto xz = v.xz();
    REQUIRE(xz.x == 1.0f);
    REQUIRE(xz.y == 3.0f);

    auto yz = v.yz();
    REQUIRE(yz.x == 2.0f);
    REQUIRE(yz.y == 3.0f);
}

TEST_CASE("vec4 default construction") {
    vec<4, float> v;
    REQUIRE(v.x == 0.0f);
    REQUIRE(v.y == 0.0f);
    REQUIRE(v.z == 0.0f);
    REQUIRE(v.w == 0.0f);
}

TEST_CASE("vec4 from vec3") {
    vec<3, float> xyz(1.0f, 2.0f, 3.0f);
    vec<4, float> v(xyz, 4.0f);
    REQUIRE(v.x == 1.0f);
    REQUIRE(v.y == 2.0f);
    REQUIRE(v.z == 3.0f);
    REQUIRE(v.w == 4.0f);
}

TEST_CASE("vec4 accessors") {
    vec<4, float> v(1.0f, 2.0f, 3.0f, 4.0f);
    REQUIRE(v.xyz().x == 1.0f);
    REQUIRE(v.xyz().y == 2.0f);
    REQUIRE(v.xyz().z == 3.0f);
}

TEST_CASE("vec functions") {
    vec<3, float> a(1.0f, 2.0f, 3.0f);
    vec<3, float> b(4.0f, 5.0f, 6.0f);

    auto mn = min(a, b);
    REQUIRE(mn.x == 1.0f);
    REQUIRE(mn.y == 2.0f);
    REQUIRE(mn.z == 3.0f);

    auto mx = max(a, b);
    REQUIRE(mx.x == 4.0f);
    REQUIRE(mx.y == 5.0f);
    REQUIRE(mx.z == 6.0f);

    auto ab = abs(vec<3, float>(-1.0f, -2.0f, -3.0f));
    REQUIRE(ab.x == 1.0f);
    REQUIRE(ab.y == 2.0f);
    REQUIRE(ab.z == 3.0f);

    auto cl = clamp(vec<3, float>(0.5f, 1.5f, 2.5f), vec<3, float>(1.0f, 1.0f, 1.0f), vec<3, float>(2.0f, 2.0f, 2.0f));
    REQUIRE(cl.x == 1.0f);
    REQUIRE(cl.y == 1.5f);
    REQUIRE(cl.z == 2.0f);

    auto lp = lerp(vec<3, float>(0.0f, 0.0f, 0.0f), vec<3, float>(10.0f, 10.0f, 10.0f), 0.5f);
    REQUIRE(lp.x == 5.0f);
    REQUIRE(lp.y == 5.0f);
    REQUIRE(lp.z == 5.0f);

    auto refl = reflect(vec<3, float>(1.0f, -1.0f, 0.0f), vec<3, float>(0.0f, 1.0f, 0.0f));
    REQUIRE(refl.x == 1.0f);
    REQUIRE(refl.y == 1.0f);
    REQUIRE(refl.z == 0.0f);

    REQUIRE(distance(vec<3, float>(0.0f, 0.0f, 0.0f), vec<3, float>(3.0f, 4.0f, 0.0f)) == Approx(5.0f));
    REQUIRE(distance_squared(vec<3, float>(0.0f, 0.0f, 0.0f), vec<3, float>(3.0f, 4.0f, 0.0f)) == 25.0f);
}

TEST_CASE("vec scalar min and max") {
    vec<3, float> a(1.0f, 5.0f, 3.0f);
    auto mn = min(a, 2.0f);
    REQUIRE(mn.x == 1.0f);
    REQUIRE(mn.y == 2.0f);
    REQUIRE(mn.z == 2.0f);

    auto mx = max(a, 2.0f);
    REQUIRE(mx.x == 2.0f);
    REQUIRE(mx.y == 5.0f);
    REQUIRE(mx.z == 3.0f);
}

TEST_CASE("vec saturate") {
    vec<3, float> v(-0.5f, 0.5f, 1.5f);
    auto s = saturate(v);
    REQUIRE(s.x == 0.0f);
    REQUIRE(s.y == 0.5f);
    REQUIRE(s.z == 1.0f);
}

TEST_CASE("vec smoothstep") {
    vec<3, float> edge0(0.0f, 0.0f, 0.0f);
    vec<3, float> edge1(1.0f, 1.0f, 1.0f);
    vec<3, float> x(0.5f, 0.0f, 1.0f);
    auto s = smoothstep(edge0, edge1, x);
    REQUIRE(s.x == Approx(0.5f));
    REQUIRE(s.y == 0.0f);
    REQUIRE(s.z == 1.0f);
}

TEST_CASE("vec refract") {
    vec<3, float> incident(1.0f, -1.0f, 0.0f);
    vec<3, float> normal(0.0f, 1.0f, 0.0f);
    auto r = refract(incident.normalized(), normal, 1.0f);
    // When eta == 1, ray continues straight
    REQUIRE(r.x == Approx(incident.normalized().x).margin(1e-5f));
    REQUIRE(r.y == Approx(incident.normalized().y).margin(1e-5f));
    REQUIRE(r.z == Approx(0.0f).margin(1e-5f));
}

TEST_CASE("vec component-wise math functions") {
    vec<3, float> v(0.0f, 1.0f, 4.0f);

    auto sq = sqrt(v);
    REQUIRE(sq.x == 0.0f);
    REQUIRE(sq.y == 1.0f);
    REQUIRE(sq.z == 2.0f);

    auto s = sin(vec<3, float>(0.0f, half_pi<float>, pi<float>));
    REQUIRE(s.x == Approx(0.0f).margin(1e-6f));
    REQUIRE(s.y == Approx(1.0f).margin(1e-6f));
    REQUIRE(s.z == Approx(0.0f).margin(1e-6f));

    auto c = cos(vec<3, float>(0.0f, half_pi<float>, pi<float>));
    REQUIRE(c.x == Approx(1.0f).margin(1e-6f));
    REQUIRE(c.y == Approx(0.0f).margin(1e-6f));
    REQUIRE(c.z == Approx(-1.0f).margin(1e-6f));

    auto p = pow(v, 2.0f);
    REQUIRE(p.x == 0.0f);
    REQUIRE(p.y == 1.0f);
    REQUIRE(p.z == 16.0f);

    auto fl = floor(vec<3, float>(1.7f, 2.3f, -1.2f));
    REQUIRE(fl.x == 1.0f);
    REQUIRE(fl.y == 2.0f);
    REQUIRE(fl.z == -2.0f);

    auto ce = ceil(vec<3, float>(1.7f, 2.3f, -1.2f));
    REQUIRE(ce.x == 2.0f);
    REQUIRE(ce.y == 3.0f);
    REQUIRE(ce.z == -1.0f);

    auto ro = round(vec<3, float>(1.4f, 1.6f, -1.5f));
    REQUIRE(ro.x == 1.0f);
    REQUIRE(ro.y == 2.0f);
    REQUIRE(ro.z == -2.0f); // std::round is half-away-from-zero
}

TEST_CASE("vec generic base class conversion") {
    vec<3, float> v(1.0f, 2.0f, 3.0f);
    vec<3, double> vd(v);
    REQUIRE(vd.x == 1.0);
    REQUIRE(vd.y == 2.0);
    REQUIRE(vd.z == 3.0);
}

TEST_CASE("project and unproject roundtrip") {
    // Ground truth: world=[1,2,0] -> ndc=[0.194856, 0.69282, 0.961962] -> back=[1, 2, ~0]
    vec<3, float> eye(0.0f, 0.0f, 5.0f);
    vec<3, float> center(0.0f, 0.0f, 0.0f);
    vec<3, float> up(0.0f, 1.0f, 0.0f);
    auto view = lookAt(eye, center, up);
    auto proj = perspective(radians(60.0f), 16.0f / 9.0f, 0.1f, 100.0f);

    vec<3, float> world(1.0f, 2.0f, 0.0f);
    auto ndc = project_ndc(world, view, proj);
    REQUIRE(ndc.x == Approx(0.19485572f).margin(1e-5f));
    REQUIRE(ndc.y == Approx(0.6928203f).margin(1e-5f));

    auto back = unproject_ndc(ndc, view, proj);
    REQUIRE(back.x == Approx(world.x).margin(1e-5f));
    REQUIRE(back.y == Approx(world.y).margin(1e-5f));
    REQUIRE(back.z == Approx(world.z).margin(1e-4f));
}
