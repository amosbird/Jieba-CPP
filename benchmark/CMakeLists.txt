cmake_minimum_required(VERSION 3.20)
project(jieba_benchmark CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(CPPJIEBA "${CMAKE_CURRENT_SOURCE_DIR}/third_party/cppjieba")
set(DATRIE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/cppjieba-datrie")
set(NEXALOID "${CMAKE_CURRENT_SOURCE_DIR}/third_party/nexaloid")
set(DARTS_INCLUDE "${ROOT}/../darts-clone/include" CACHE PATH "darts-clone include directory")

add_library(jieba_cpp STATIC
    "${ROOT}/src/jieba.cpp"
    "${ROOT}/src/jieba_dict.cpp"
    "${ROOT}/src/jieba_mp_seg.cpp"
    "${ROOT}/src/jieba_hmm_seg.cpp")
target_include_directories(jieba_cpp PUBLIC "${ROOT}/include" "${DARTS_INCLUDE}" PRIVATE "${ROOT}/data")
target_compile_options(jieba_cpp PRIVATE "--embed-dir=${ROOT}/data")
target_link_libraries(jieba_cpp PRIVATE zstd)

add_executable(bench_jieba_cpp jieba_cpp_runner.cpp)
target_link_libraries(bench_jieba_cpp PRIVATE jieba_cpp)

add_executable(bench_cppjieba cppjieba_runner.cpp)
target_include_directories(bench_cppjieba PRIVATE "${CPPJIEBA}/include" "${CPPJIEBA}/deps/limonp/include")
target_compile_definitions(bench_cppjieba PRIVATE
    IMPLEMENTATION_NAME="cppjieba"
    DICT_PATH="${CPPJIEBA}/dict/jieba.dict.utf8"
    HMM_PATH="${CPPJIEBA}/dict/hmm_model.utf8"
    USER_DICT_PATH="")

add_executable(bench_cppjieba_datrie cppjieba_runner.cpp "${DATRIE}/deps/limonp/Md5.cpp")
target_include_directories(bench_cppjieba_datrie PRIVATE
    "${DATRIE}/include" "${DATRIE}/deps" "${DATRIE}/deps/darts-clone/include")
target_compile_definitions(bench_cppjieba_datrie PRIVATE
    CPPJIEBA_DATRIE=1
    IMPLEMENTATION_NAME="cppjieba-datrie"
    DICT_PATH="${CPPJIEBA}/dict/jieba.dict.utf8"
    HMM_PATH="${CPPJIEBA}/dict/hmm_model.utf8"
    USER_DICT_PATH=""
    DAT_CACHE_PATH="${CMAKE_CURRENT_BINARY_DIR}/cppjieba-datrie.cache")

add_executable(bench_nexaloid nexaloid_runner.cpp)
target_include_directories(bench_nexaloid PRIVATE "${NEXALOID}/core/include")
target_link_directories(bench_nexaloid PRIVATE "${NEXALOID}/core/zig-out/lib")
target_link_libraries(bench_nexaloid PRIVATE nexaloid)
target_compile_definitions(bench_nexaloid PRIVATE
    NEXALOID_DICT_PATH="${NEXALOID}/data/dict/nexaloid.nxdict"
    NEXALOID_PLUGIN_PATH="${NEXALOID}/libnexaloid_plugin_hmm_lattice.so"
    NEXALOID_HMM_PATH="${NEXALOID}/data/hmm/bmes_hmm_wordhub_lattice.nxhmm")
set_target_properties(bench_nexaloid PROPERTIES BUILD_RPATH "${NEXALOID}/core/zig-out/lib")

foreach(target bench_jieba_cpp bench_cppjieba bench_cppjieba_datrie bench_nexaloid)
    target_compile_options(${target} PRIVATE -O3 -DNDEBUG -march=native)
endforeach()
