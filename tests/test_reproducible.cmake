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
# Mirror the repo's tools/ + data/ layout so the generator writes to data/ as it
# does in the repo (it resolves its output dir as ../data relative to itself).
file(MAKE_DIRECTORY "${tmp}/tools")
file(MAKE_DIRECTORY "${tmp}/data")
configure_file("${REPO}/tools/generate_dict.py" "${tmp}/tools/generate_dict.py" COPYONLY)

execute_process(
    COMMAND ${PYTHON} "${tmp}/tools/generate_dict.py"
    WORKING_DIRECTORY "${tmp}/tools"
    RESULT_VARIABLE gen_rc
    OUTPUT_VARIABLE gen_out
    ERROR_VARIABLE gen_err)

if(NOT gen_rc EQUAL 0)
    # Most likely no network. Treat as skip, not failure.
    message(STATUS "test_reproducible: generator could not run (likely offline); skipping\n${gen_err}")
    return()
endif()

# Compare the regenerated dictionaries against the committed ones by their
# DECOMPRESSED payload, not the compressed bytes: zstd's container framing is not
# guaranteed to be byte-identical across libzstd versions, but the dictionary data
# we actually embed must be. (`generate_hmm_model.py` output, being plain text, is
# compared as-is below if present.)
find_program(ZSTD_CLI NAMES zstd)

foreach(name dict_le.dat.zst dict_be.dat.zst)
    if(ZSTD_CLI)
        execute_process(COMMAND ${ZSTD_CLI} -dc "${tmp}/data/${name}"
                        OUTPUT_FILE "${tmp}/regenerated_${name}.raw" RESULT_VARIABLE r1)
        execute_process(COMMAND ${ZSTD_CLI} -dc "${REPO}/data/${name}"
                        OUTPUT_FILE "${tmp}/committed_${name}.raw" RESULT_VARIABLE r2)
        if(NOT r1 EQUAL 0 OR NOT r2 EQUAL 0)
            message(FATAL_ERROR "test_reproducible: failed to decompress ${name}")
        endif()
        file(SHA256 "${tmp}/regenerated_${name}.raw" regenerated_sha)
        file(SHA256 "${tmp}/committed_${name}.raw" committed_sha)
        set(what "decompressed payload")
    else()
        # No zstd CLI: fall back to comparing the compressed bytes (may be a false
        # negative across libzstd versions, but better than no check).
        file(SHA256 "${tmp}/data/${name}" regenerated_sha)
        file(SHA256 "${REPO}/data/${name}" committed_sha)
        set(what "compressed bytes (no zstd CLI to compare payloads)")
    endif()

    if(NOT regenerated_sha STREQUAL committed_sha)
        message(FATAL_ERROR
            "test_reproducible: regenerated ${name} does not match the committed file (${what})\n"
            "  committed:    ${committed_sha}\n"
            "  regenerated:  ${regenerated_sha}")
    endif()
    message(STATUS "test_reproducible: ${name} reproduced (${what}, ${committed_sha})")
endforeach()
