#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <woki/math/mat/mat2.hpp>
#include <woki/math/mat/mat3.hpp>
#include <woki/math/mat/mat4.hpp>
#include <woki/math/mat/functions.hpp>

using namespace woki::math;
using Catch::Approx;

TEST_CASE("mat2 identity") {
    auto m = mat<2, 2, float>::identity();
    REQUIRE(m(0, 0) == 1.0f);
    REQUIRE(m(1, 1) == 1.0f);
    REQUIRE(m(0, 1) == 0.0f);
    REQUIRE(m(1, 0) == 0.0f);
}

TEST_CASE("mat2 determinant") {
    mat<2, 2, float> m(layout::rowm, 1.0f, 2.0f, 3.0f, 4.0f);
    REQUIRE(m.det() == Approx(-2.0f));
}

TEST_CASE("mat2 inverse exact values") {
    mat<2, 2, float> m(layout::rowm, 4.0f, 7.0f, 2.0f, 6.0f);
    auto inv = m.inverse();
    // Ground truth: [[0.6, -0.7], [-0.2, 0.4]]
    REQUIRE(inv(0, 0) == Approx(0.6f));
    REQUIRE(inv(0, 1) == Approx(-0.7f));
    REQUIRE(inv(1, 0) == Approx(-0.2f));
    REQUIRE(inv(1, 1) == Approx(0.4f));

    auto prod = m * inv;
    REQUIRE(prod(0, 0) == Approx(1.0f).margin(1e-6f));
    REQUIRE(prod(1, 1) == Approx(1.0f).margin(1e-6f));
    REQUIRE(prod(0, 1) == Approx(0.0f).margin(1e-6f));
    REQUIRE(prod(1, 0) == Approx(0.0f).margin(1e-6f));
}

TEST_CASE("mat2 col and row access") {
    mat<2, 2, float> m(layout::rowm, 1.0f, 2.0f, 3.0f, 4.0f);
    auto col0 = m.col(0);
    REQUIRE(col0[0] == 1.0f);
    REQUIRE(col0[1] == 3.0f);

    auto row0 = m.row(0);
    REQUIRE(row0[0] == 1.0f);
    REQUIRE(row0[1] == 2.0f);

    m.set_col(1, {5.0f, 6.0f});
    REQUIRE(m(0, 1) == 5.0f);
    REQUIRE(m(1, 1) == 6.0f);

    m.set_row(1, {7.0f, 8.0f});
    REQUIRE(m(1, 0) == 7.0f);
    REQUIRE(m(1, 1) == 8.0f);
}

TEST_CASE("mat4 identity") {
    auto m = mat<4, 4, float>::identity();
    for (std::size_t i = 0; i < 4; ++i) {
        for (std::size_t j = 0; j < 4; ++j) {
            if (i == j)
                REQUIRE(m(i, j) == 1.0f);
            else
                REQUIRE(m(i, j) == 0.0f);
        }
    }
}

TEST_CASE("mat4 element access") {
    mat<4, 4, float> m(layout::rowm, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f);

    REQUIRE(m(0, 0) == 1.0f);
    REQUIRE(m(0, 1) == 2.0f);
    REQUIRE(m(1, 0) == 5.0f);
    REQUIRE(m(1, 1) == 6.0f);
    REQUIRE(m(3, 3) == 16.0f);
}

TEST_CASE("mat4 col and row access") {
    mat<4, 4, float> m(layout::rowm, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f);

    auto col0 = m.col(0);
    REQUIRE(col0[0] == 1.0f);
    REQUIRE(col0[1] == 5.0f);

    m.set_col(1, {0.0f, 1.0f, 0.0f, 0.0f});
    REQUIRE(m(1, 1) == 1.0f);

    auto row1 = m.row(1);
    REQUIRE(row1[0] == 5.0f);
    REQUIRE(row1[1] == 1.0f);

    m.set_row(2, {8.0f, 7.0f, 6.0f, 5.0f});
    REQUIRE(m(2, 0) == 8.0f);
    REQUIRE(m(2, 3) == 5.0f);
}

TEST_CASE("mat4 scalar multiplication") {
    auto m = mat<4, 4, float>::identity();
    auto scaled = m * 5.0f;
    REQUIRE(scaled(0, 0) == 5.0f);
    REQUIRE(scaled(1, 1) == 5.0f);
    REQUIRE(scaled(2, 2) == 5.0f);
    REQUIRE(scaled(3, 3) == 5.0f);
}

TEST_CASE("mat4 matrix multiplication") {
    mat<4, 4, float> a(layout::rowm, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 2.0f, 0.0f, 0.0f, 0.0f, 0.0f, 3.0f, 0.0f, 0.0f, 0.0f, 0.0f, 4.0f);

    mat<4, 4, float> b(layout::rowm, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);

    auto c = a * b;
    REQUIRE(c(0, 0) == 1.0f);
    REQUIRE(c(1, 1) == 2.0f);
    REQUIRE(c(2, 2) == 3.0f);
    REQUIRE(c(3, 3) == 4.0f);
}

TEST_CASE("mat4 transpose") {
    mat<4, 4, float> m(layout::rowm, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f);

    auto t = m.transpose();
    REQUIRE(t(0, 1) == 5.0f);
    REQUIRE(t(1, 0) == 2.0f);
    REQUIRE(t(2, 3) == 15.0f);
    REQUIRE(t(3, 2) == 12.0f);
}

TEST_CASE("mat4 determinant") {
    mat<4, 4, float> m = mat<4, 4, float>::identity();
    REQUIRE(m.det() == Approx(1.0f));

    mat<4, 4, float> s(layout::rowm, 2.0f, 0.0f, 0.0f, 0.0f, 0.0f, 3.0f, 0.0f, 0.0f, 0.0f, 0.0f, 4.0f, 0.0f, 0.0f, 0.0f, 0.0f, 5.0f);
    REQUIRE(s.det() == Approx(120.0f));
}

TEST_CASE("mat4 translate inverse exact values") {
    mat<4, 4, float> m(layout::rowm, 1.0f, 0.0f, 0.0f, 2.0f, 0.0f, 1.0f, 0.0f, 3.0f, 0.0f, 0.0f, 1.0f, 4.0f, 0.0f, 0.0f, 0.0f, 1.0f);
    // Ground truth: inverse(translate) = translate(-tx, -ty, -tz)
    auto inv = m.inverse();
    REQUIRE(inv(0, 3) == Approx(-2.0f));
    REQUIRE(inv(1, 3) == Approx(-3.0f));
    REQUIRE(inv(2, 3) == Approx(-4.0f));

    auto prod = m * inv;
    for (std::size_t i = 0; i < 4; ++i) {
        for (std::size_t j = 0; j < 4; ++j) {
            if (i == j)
                REQUIRE(prod(i, j) == Approx(1.0f).margin(1e-6f));
            else
                REQUIRE(prod(i, j) == Approx(0.0f).margin(1e-6f));
        }
    }
}

TEST_CASE("mat4 generic member det and inverse") {
    // Use the generic base class for a non-specialized size
    // (Testing that .det() and .inverse() members compile and work)
    mat<2, 2, float> m(layout::rowm, 2.0f, 0.0f, 0.0f, 3.0f);
    REQUIRE(m.det() == Approx(6.0f));
    auto inv = m.inverse();
    REQUIRE(inv(0, 0) == Approx(0.5f));
    REQUIRE(inv(1, 1) == Approx(1.0f / 3.0f));
}

TEST_CASE("mat3 identity and determinant") {
    auto m = mat<3, 3, float>::identity();
    REQUIRE(m.det() == Approx(1.0f));

    mat<3, 3, float> a(layout::rowm, 2.0f, 0.0f, 0.0f, 0.0f, 3.0f, 0.0f, 0.0f, 0.0f, 4.0f);
    REQUIRE(a.det() == Approx(24.0f));
}

TEST_CASE("mat3 inverse exact values") {
    mat<3, 3, float> m(layout::rowm, 1.0f, 2.0f, 3.0f, 0.0f, 1.0f, 4.0f, 5.0f, 6.0f, 0.0f);
    // Ground truth: det=1, inverse has exact integer entries
    REQUIRE(m.det() == Approx(1.0f));

    auto inv = m.inverse();
    REQUIRE(inv(0, 0) == Approx(-24.0f));
    REQUIRE(inv(0, 1) == Approx(18.0f));
    REQUIRE(inv(0, 2) == Approx(5.0f));
    REQUIRE(inv(1, 0) == Approx(20.0f));
    REQUIRE(inv(1, 1) == Approx(-15.0f));
    REQUIRE(inv(1, 2) == Approx(-4.0f));
    REQUIRE(inv(2, 0) == Approx(-5.0f));
    REQUIRE(inv(2, 1) == Approx(4.0f));
    REQUIRE(inv(2, 2) == Approx(1.0f));

    auto prod = m * inv;
    for (std::size_t i = 0; i < 3; ++i) {
        for (std::size_t j = 0; j < 3; ++j) {
            if (i == j)
                REQUIRE(prod(i, j) == Approx(1.0f).margin(1e-5f));
            else
                REQUIRE(prod(i, j) == Approx(0.0f).margin(1e-5f));
        }
    }
}

TEST_CASE("mat3 col and row access") {
    mat<3, 3, float> m = mat<3, 3, float>::identity();
    auto col0 = m.col(0);
    REQUIRE(col0[0] == 1.0f);
    REQUIRE(col0[1] == 0.0f);
    REQUIRE(col0[2] == 0.0f);

    m.set_col(1, {0.0f, 1.0f, 0.0f});
    REQUIRE(m(1, 1) == 1.0f);

    auto row2 = m.row(2);
    REQUIRE(row2[2] == 1.0f);

    m.set_row(0, {5.0f, 0.0f, 0.0f});
    REQUIRE(m(0, 0) == 5.0f);
}

TEST_CASE("mat3 comparison") {
    mat<3, 3, float> a = mat<3, 3, float>::identity();
    mat<3, 3, float> b = mat<3, 3, float>::identity();
    mat<3, 3, float> c(layout::rowm, 1.0f, 0.0f, 0.0f, 0.0f, 2.0f, 0.0f, 0.0f, 0.0f, 1.0f);

    REQUIRE(a == b);
    REQUIRE(a != c);
}

TEST_CASE("mat trace") {
    mat<4, 4, float> m(layout::rowm, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f);
    REQUIRE(trace(m) == 1.0f + 6.0f + 11.0f + 16.0f);
}
