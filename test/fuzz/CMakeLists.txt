#
# libFuzzer targets (Clang only), enabled with -DOWL_FUZZING=ON; combine with -DOWL_ENABLE_ADDRESS_SANITIZER=ON.
# Run: bin/owl_pack_reader_fuzzer -max_total_time=300 <corpus_dir>
#
file(GLOB_RECURSE FUZZ_SRCS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*_fuzzer.cpp)
foreach (fuzzSrc ${FUZZ_SRCS})
    get_filename_component(fuzzName ${fuzzSrc} NAME_WE)
    set(fuzzTarget ${PROJECT_PREFIX_LOWER}_${fuzzName})
    add_executable(${fuzzTarget} ${fuzzSrc})
    set_target_properties(${fuzzTarget} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
            FOLDER "Fuzz")
    target_link_libraries(${fuzzTarget} PRIVATE ${ENGINE_NAME} ${ENGINE_NAME}Private)
    target_link_options(${fuzzTarget} PRIVATE -fsanitize=fuzzer)
endforeach ()
