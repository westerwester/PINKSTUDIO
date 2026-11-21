add_executable(pinkstudio_tests
    test_main.cpp
    test_core.cpp
    test_dsp.cpp
    test_meters.cpp
    test_instruments.cpp
    test_engine.cpp
)

target_link_libraries(pinkstudio_tests PRIVATE pinkstudio_engine)
find_package(Threads REQUIRED)
target_link_libraries(pinkstudio_tests PRIVATE Threads::Threads)

if(MSVC)
    target_compile_options(pinkstudio_tests PRIVATE /W3 /utf-8)
endif()

add_test(NAME pinkstudio_tests COMMAND pinkstudio_tests)
