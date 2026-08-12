#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <woki/gfx/camera.hpp>

using Catch::Approx;
using namespace woki;

TEST_CASE("WebGPU camera projection has zero-to-one depth") {
    const auto perspective = gfx::PerspectiveWebGpuRH(math::radians(60.0f), 1.5f, 0.1f, 100.0f);
    REQUIRE(perspective);
    const math::vec4f near_clip = *perspective * math::vec4f{0.0f, 0.0f, -0.1f, 1.0f};
    const math::vec4f far_clip = *perspective * math::vec4f{0.0f, 0.0f, -100.0f, 1.0f};
    CHECK(near_clip.z / near_clip.w == Approx(0.0f).margin(1.0e-5f));
    CHECK(far_clip.z / far_clip.w == Approx(1.0f).margin(1.0e-5f));

    const auto orthographic = gfx::OrthographicWebGpuRH(4.0f, 1.0f, 0.1f, 100.0f);
    REQUIRE(orthographic);
    CHECK(((*orthographic * math::vec4f{0.0f, 0.0f, -0.1f, 1.0f}).z) == Approx(0.0f).margin(1.0e-5f));
    CHECK(((*orthographic * math::vec4f{0.0f, 0.0f, -100.0f, 1.0f}).z) == Approx(1.0f).margin(1.0e-5f));
    CHECK_FALSE(gfx::PerspectiveWebGpuRH(0.0f, 1.0f, 0.1f, 10.0f));
}

TEST_CASE("Camera pose has an explicit right-handed identity basis and view") {
    gfx::CameraPose pose;
    CHECK(pose.Forward() == math::vec3f{0.0f, 0.0f, -1.0f});
    CHECK(pose.Right() == math::vec3f{1.0f, 0.0f, 0.0f});
    CHECK(pose.Up() == math::vec3f{0.0f, 1.0f, 0.0f});
    pose.position = {2.0f, 3.0f, 4.0f};
    REQUIRE(pose.LookAt({2.0f, 3.0f, 3.0f}));
    const auto view = pose.ViewMatrix();
    REQUIRE(view);
    const math::vec4f eye = *view * math::vec4f{pose.position, 1.0f};
    CHECK(eye.x == Approx(0.0f));
    CHECK(eye.y == Approx(0.0f));
    CHECK(eye.z == Approx(0.0f));
    CHECK_FALSE(pose.LookAt(pose.position));
    CHECK_FALSE(pose.LookAt({2.0f, 3.0f, 2.0f}, {0.0f, 0.0f, -2.0f}));
}

TEST_CASE("Physical lens conversion round trips") {
    gfx::PerspectiveCamera camera;
    camera.sensor_height_mm = 24.0f;
    REQUIRE(camera.SetFocalLengthMm(50.0f));
    const auto focal_length = camera.FocalLengthMm();
    REQUIRE(focal_length);
    CHECK(*focal_length == Approx(50.0f).epsilon(1.0e-5f));
}

TEST_CASE("Normalized viewports completely partition odd framebuffers and hit test HiDPI") {
    const gfx::CameraViewport left{0.0f, 0.0f, 0.5f, 1.0f};
    const gfx::CameraViewport right{0.5f, 0.0f, 0.5f, 1.0f};
    const auto a = left.Resolve(101, 51);
    const auto b = right.Resolve(101, 51);
    REQUIRE(a);
    REQUIRE(b);
    CHECK(a->x + a->width == b->x);
    CHECK(a->width + b->width == 101);
    REQUIRE(left.HitTest(10.0f, 10.0f, 2.0f, 2.0f, 101, 51));
    CHECK(*left.HitTest(10.0f, 10.0f, 2.0f, 2.0f, 101, 51));
    CHECK_FALSE(*left.HitTest(30.0f, 10.0f, 2.0f, 2.0f, 101, 51));
}

TEST_CASE("Orbit focus and clamps produce a valid camera") {
    gfx::OrbitController orbit;
    orbit.min_distance = 2.0f;
    orbit.max_distance = 5.0f;
    REQUIRE(orbit.Focus({1.0f, 2.0f, 3.0f}, 100.0f));
    CHECK(orbit.distance == Approx(5.0f));
    orbit.Rotate(0.0f, -100000.0f);
    CHECK(orbit.pitch == Approx(orbit.max_pitch));
    orbit.Dolly(-1000.0f);
    CHECK(orbit.distance == Approx(orbit.max_distance));
    gfx::CameraPose pose;
    REQUIRE(orbit.Update(pose));
    CHECK((pose.position - orbit.target).length() == Approx(orbit.distance));
}

TEST_CASE("Fly movement is deterministic and follows camera forward") {
    gfx::FlyController a;
    gfx::FlyController b;
    gfx::CameraPose pose_a;
    gfx::CameraPose pose_b;
    const gfx::FlyInput input{.forward = 1.0f};
    REQUIRE(a.Update(pose_a, input, 0.1f));
    REQUIRE(b.Update(pose_b, input, 0.1f));
    CHECK(pose_a.position == pose_b.position);
    CHECK(pose_a.position.z < 0.0f);
}

TEST_CASE("Camera view projects, unprojects, and constructs rays") {
    gfx::CameraPose pose;
    pose.position = {0.0f, 0.0f, 5.0f};
    REQUIRE(pose.LookAt({}));
    const auto view = gfx::CameraView::Build(pose, gfx::PerspectiveCamera{}, {}, 800, 600);
    REQUIRE(view);
    const auto screen = view->Project({0.0f, 0.0f, 0.0f});
    REQUIRE(screen);
    CHECK(screen->x == Approx(400.0f));
    CHECK(screen->y == Approx(300.0f));
    const auto world = view->Unproject(*screen);
    REQUIRE(world);
    CHECK(world->x == Approx(0.0f).margin(1.0e-4f));
    CHECK(world->y == Approx(0.0f).margin(1.0e-4f));
    CHECK(world->z == Approx(0.0f).margin(1.0e-3f));
    const auto ray = view->Ray(400.0f, 300.0f);
    REQUIRE(ray);
    CHECK(ray->origin == pose.position);
    CHECK(ray->direction.z == Approx(-1.0f));
}

TEST_CASE("Camera jitter translates perspective and orthographic projections in NDC") {
    gfx::CameraPose pose;
    pose.position = {0.0f, 0.0f, 5.0f};
    REQUIRE(pose.LookAt({}));
    constexpr math::vec2f jitter{0.25f, -0.5f};

    for (const gfx::CameraProjection projection : {gfx::CameraProjection{gfx::PerspectiveCamera{}}, gfx::CameraProjection{gfx::OrthographicCamera{}}}) {
        const auto view = gfx::CameraView::Build(pose, projection, {}, 800, 600, jitter);
        REQUIRE(view);
        const auto screen = view->Project({});
        REQUIRE(screen);
        CHECK(screen->x == Approx(500.0f).margin(1.0e-3f));
        CHECK(screen->y == Approx(450.0f).margin(1.0e-3f));
        const auto world = view->Unproject(*screen);
        REQUIRE(world);
        CHECK(world->x == Approx(0.0f).margin(1.0e-3f));
        CHECK(world->y == Approx(0.0f).margin(1.0e-3f));
        CHECK(world->z == Approx(0.0f).margin(1.0e-3f));
    }
}

TEST_CASE("Large orthographic cameras retain reliable inverse matrices") {
    gfx::OrthographicCamera projection;
    projection.height = 1.0e8f;
    projection.far_plane = 1.0e8f;
    const auto view = gfx::CameraView::Build({}, projection, {}, 1920, 1080);
    REQUIRE(view);
    const auto screen = view->Project({1.0e7f, -2.0e7f, -5.0e7f});
    REQUIRE(screen);
    const auto world = view->Unproject(*screen);
    REQUIRE(world);
    CHECK(world->x == Approx(1.0e7f).epsilon(1.0e-5f));
    CHECK(world->y == Approx(-2.0e7f).epsilon(1.0e-5f));
    CHECK(world->z == Approx(-5.0e7f).epsilon(1.0e-5f));
}
