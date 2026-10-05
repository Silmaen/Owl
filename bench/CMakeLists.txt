#
# Owl micro-benchmarks (in-house harness: no Google Benchmark package is available in DepManager).
#
set(OWL_BENCH ${PROJECT_PREFIX_LOWER}_bench)
file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB_RECURSE HDRS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
add_executable(${OWL_BENCH} ${SRCS} ${HDRS})
set_target_properties(${OWL_BENCH} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        FOLDER "Bench")
target_include_directories(${OWL_BENCH} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
# The private interface exposes engine internals (shader utilities) like the unit tests do.
target_link_libraries(${OWL_BENCH} PRIVATE ${ENGINE_NAME} ${ENGINE_NAME}Private)
target_compile_definitions(${OWL_BENCH} PRIVATE OWL_BENCH_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
