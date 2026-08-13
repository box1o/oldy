#include <sstream>
#include <catch2/catch_test_macros.hpp>

#include <woki/math/mat/mat4.hpp>
#include <woki/math/vec/vec3.hpp>
#include <woki/math/common/io.hpp>
#include <woki/math/quat/quat.hpp>

using namespace woki::math;

TEST_CASE("ostream vec3") {
    vec<3, float> v(1.0f, 2.0f, 3.0f);
    std::ostringstream oss;
    oss << v;
    REQUIRE(oss.str() == "vec3(1, 2, 3)");
}

TEST_CASE("ostream mat4 identity") {
    auto m = mat<4, 4, float>::identity();
    std::ostringstream oss;
    oss << m;
    REQUIRE(oss.str() == "mat4x4([1, 0, 0, 0], [0, 1, 0, 0], [0, 0, 1, 0], [0, 0, 0, 1])");
}

TEST_CASE("ostream quat") {
    quat<float> q(1.0f, 2.0f, 3.0f, 4.0f);
    std::ostringstream oss;
    oss << q;
    REQUIRE(oss.str() == "quat(1, 2, 3, 4)");
}
