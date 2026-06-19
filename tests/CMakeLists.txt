# Unit tests use a tiny header-only check helper (no external test framework).
# Each test is a standalone executable returning non-zero on failure.

add_library(jiebacpp_test_main INTERFACE)
target_include_directories(jiebacpp_test_main INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

function(jiebacpp_add_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE JiebaCPP::jieba jiebacpp_test_main)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

jiebacpp_add_test(test_segmentation)
jiebacpp_add_test(test_encoding)

# Endianness mirror check: validates that dict_be.dat.zst is the exact element-wise
# byte-swap of dict_le.dat.zst. Needs zstd and the data directory; runs on any host.
add_executable(test_endianness test_endianness.cpp)
target_link_libraries(test_endianness PRIVATE jiebacpp_test_main jiebacpp_zstd)
add_test(NAME test_endianness
         COMMAND test_endianness ${CMAKE_SOURCE_DIR}/data)

# Reproducibility check: re-run the generators and diff against the committed data.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(
        NAME test_reproducible
        COMMAND ${CMAKE_COMMAND}
                -DPYTHON=${Python3_EXECUTABLE}
                -DREPO=${CMAKE_SOURCE_DIR}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/test_reproducible.cmake)
endif()
