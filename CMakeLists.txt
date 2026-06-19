cmake_minimum_required(VERSION 3.16)
project(JiebaCPP CXX)

# Jieba-CPP — a compact C++ reimplementation of the jieba Chinese word
# segmentation algorithm, with an embedded dictionary and HMM model.
#
# The library sources and headers are intentionally byte-identical to the copy
# vendored in ClickHouse (contrib). This standalone build exists so the library
# can be developed, tested and consumed independently; ClickHouse builds it
# through its own `-cmake` wrapper using its in-tree dependencies.

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

option(JIEBACPP_BUILD_TESTS "Build Jieba-CPP unit tests" ON)
# abseil's ABI is sensitive to compiler flags (notably sanitizers and the
# debug-hardening mode), so mixing a system abseil with a sanitized build of
# this library leads to spurious failures. Set this ON to build abseil from
# source with matching flags (the CI sanitizer job does this).
option(JIEBACPP_FETCH_ABSEIL "Build abseil from source instead of find_package" OFF)

include(FetchContent)

# --- Dependencies -----------------------------------------------------------
#
# The library needs three things:
#   * zstd          — to decompress the embedded dictionary at load time
#   * abseil         — flat_hash_set for the separator lookup on the hot path
#   * darts-clone    — the double-array trie (header-only: a single darts.h)
#
# Prefer system packages; fall back to fetching pinned sources so a clean
# checkout builds with no manual setup.

# zstd ships its CMake config under several target spellings depending on the
# platform (`zstd::libzstd`, `zstd::libzstd_shared`, `zstd::libzstd_static`),
# and on many distros there is no CMake config at all (only the library). Probe
# config first, otherwise fall back to plain library discovery, and normalize
# everything to a single `JiebaCPP::zstd` target the library links against.
add_library(jiebacpp_zstd INTERFACE)
find_package(zstd QUIET)
if(TARGET zstd::libzstd)
    target_link_libraries(jiebacpp_zstd INTERFACE zstd::libzstd)
elseif(TARGET zstd::libzstd_shared)
    target_link_libraries(jiebacpp_zstd INTERFACE zstd::libzstd_shared)
elseif(TARGET zstd::libzstd_static)
    target_link_libraries(jiebacpp_zstd INTERFACE zstd::libzstd_static)
else()
    find_library(ZSTD_LIBRARY NAMES zstd)
    find_path(ZSTD_INCLUDE_DIR NAMES zstd.h)
    if(NOT ZSTD_LIBRARY OR NOT ZSTD_INCLUDE_DIR)
        message(FATAL_ERROR "libzstd not found. Install zstd development files (e.g. libzstd-dev).")
    endif()
    target_include_directories(jiebacpp_zstd INTERFACE ${ZSTD_INCLUDE_DIR})
    target_link_libraries(jiebacpp_zstd INTERFACE ${ZSTD_LIBRARY})
endif()

if(JIEBACPP_FETCH_ABSEIL)
    set(absl_FOUND FALSE)
else()
    find_package(absl QUIET)
endif()
if(NOT absl_FOUND)
    FetchContent_Declare(
        abseil
        GIT_REPOSITORY https://github.com/abseil/abseil-cpp.git
        GIT_TAG        20240722.0
    )
    set(ABSL_PROPAGATE_CXX_STD ON)
    set(ABSL_ENABLE_INSTALL OFF)
    FetchContent_MakeAvailable(abseil)
endif()

# darts-clone is header-only; vendor just the header via FetchContent.
FetchContent_Declare(
    darts_clone
    GIT_REPOSITORY https://github.com/s-yata/darts-clone.git
    GIT_TAG        e40ce4627526985a7767444b6ed6893ab6ff8983
)
FetchContent_MakeAvailable(darts_clone)

# --- Library ----------------------------------------------------------------

add_library(jieba
    src/jieba.cpp
    src/jieba_dict.cpp
    src/jieba_mp_seg.cpp
    src/jieba_hmm_seg.cpp)

target_compile_features(jieba PUBLIC cxx_std_23)

# Public headers in include/; the embedded data (dict_le.dat.zst via #embed and
# jieba_hmm_model.dat via #include) lives in data/ and is resolved as a quoted
# include from the sources.
target_include_directories(jieba
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/data)

# `#embed` needs its search directory passed explicitly on current compilers.
target_compile_options(jieba PRIVATE "--embed-dir=${CMAKE_CURRENT_SOURCE_DIR}/data")

target_link_libraries(jieba
    PUBLIC  absl::flat_hash_set
    PRIVATE jiebacpp_zstd)
# `jieba.h` -> `jieba_dict.h` includes <darts.h>, so darts-clone is a public
# (transitive) include dependency for consumers of the library.
target_include_directories(jieba SYSTEM PUBLIC ${darts_clone_SOURCE_DIR}/include)

add_library(JiebaCPP::jieba ALIAS jieba)

# --- Tests ------------------------------------------------------------------

if(JIEBACPP_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
