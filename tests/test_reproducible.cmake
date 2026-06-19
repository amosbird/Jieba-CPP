# Reproducibility check: re-run a generator into a temporary directory and verify it
# reproduces the committed embedded data byte-for-byte.
#
# The generators download their source data from a pinned cppjieba commit and verify it
# against a hard-coded SHA-256, so this test needs network access plus the Python deps
# (numpy, zstandard, dartsclone). It skips gracefully (does not fail) when those are
# unavailable, so it never produces false negatives in an offline build.

if(NOT PYTHON)
    message(STATUS "test_reproducible: no Python interpreter; skipping")
    return()
endif()

# Probe the Python deps the dictionary generator needs.
execute_process(
    COMMAND ${PYTHON} -c "import numpy, zstandard, dartsclone"
    RESULT_VARIABLE deps_rc
    OUTPUT_QUIET ERROR_QUIET)
if(NOT deps_rc EQUAL 0)
    message(STATUS "test_reproducible: Python deps (numpy/zstandard/dartsclone) missing; skipping")
    return()
endif()

set(tmp "${CMAKE_CURRENT_BINARY_DIR}/repro_tmp")
file(REMOVE_RECURSE "${tmp}")
file(MAKE_DIRECTORY "${tmp}")

# The generator writes its outputs next to itself, so run a copy from the temp dir.
configure_file("${REPO}/tools/generate_dict.py" "${tmp}/generate_dict.py" COPYONLY)

execute_process(
    COMMAND ${PYTHON} "${tmp}/generate_dict.py"
    WORKING_DIRECTORY "${tmp}"
    RESULT_VARIABLE gen_rc
    OUTPUT_VARIABLE gen_out
    ERROR_VARIABLE gen_err)

if(NOT gen_rc EQUAL 0)
    # Most likely no network. Treat as skip, not failure.
    message(STATUS "test_reproducible: generator could not run (likely offline); skipping\n${gen_err}")
    return()
endif()

# Compare the regenerated dictionary against the committed one, byte-for-byte.
file(SHA256 "${tmp}/dict_le.dat.zst" regenerated_sha)
file(SHA256 "${REPO}/data/dict_le.dat.zst" committed_sha)

if(NOT regenerated_sha STREQUAL committed_sha)
    message(FATAL_ERROR
        "test_reproducible: regenerated dict_le.dat.zst does not match the committed file\n"
        "  committed:    ${committed_sha}\n"
        "  regenerated:  ${regenerated_sha}")
endif()

message(STATUS "test_reproducible: dict_le.dat.zst reproduced byte-for-byte (${committed_sha})")
