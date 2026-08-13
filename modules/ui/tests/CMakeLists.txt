file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS *.cpp)

add_module_test(ui_tests
    SOURCES ${SOURCES}
    LIBRARIES woki::ui Catch2::Catch2WithMain
)
