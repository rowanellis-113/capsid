cmake_minimum_required(VERSION 3.20)

foreach(required CAPSID_SOURCE_DIR CAPSID_WORK_DIR CAPSID_TEST_GENERATOR)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "build identity git refresh test requires ${required}")
    endif()
endforeach()

find_program(GIT_EXE git REQUIRED)

string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef run_id)
get_filename_component(work_root "${CAPSID_WORK_DIR}" ABSOLUTE)
set(run "${work_root}/run-${run_id}")
set(repo "${run}/repo")
set(build "${run}/build")
file(MAKE_DIRECTORY "${repo}")

function(run_checked_git)
    execute_process(
        COMMAND "${GIT_EXE}" -C "${repo}" ${ARGN}
        RESULT_VARIABLE rc
        OUTPUT_VARIABLE out
        ERROR_VARIABLE err
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT rc EQUAL 0)
        message(FATAL_ERROR "git ${ARGN} failed (${rc}):\n${out}\n${err}")
    endif()
endfunction()

run_checked_git(init -b main)
run_checked_git(config user.name "Capsid Build Identity Test")
run_checked_git(config user.email "capsid-test@example.invalid")

string(CONCAT mini_cmakelists
    "cmake_minimum_required(VERSION 3.20)\n"
    "project(CapsidBuildIdentityRefresh NONE)\n"
    "set(CAPSID_BUILD_WORKER OFF)\n"
    "include(\"${CAPSID_SOURCE_DIR}/cmake/ComputeBuildIdentity.cmake\")\n"
    "capsid_generate_build_identity(\n"
    "    OUT_HEADER \"\${CMAKE_CURRENT_BINARY_DIR}/generated/capsid_build_identity.h\"\n"
    "    LOCKED_MANIFEST \"${CAPSID_SOURCE_DIR}/docs/data/txiki-upgrade-baseline.json\"\n"
    "    CAPSID_SOURCE_DIR \"\${CMAKE_CURRENT_SOURCE_DIR}\"\n"
    "    RUNTIME_VERSION \"0.3.2\"\n"
    "    ABI_VERSION \"7\"\n"
    "    FETCHRPC_VERSION \"4\"\n"
    "    COMPILE_FLAGS \"build_type=Release lto=OFF asan=OFF ubsan=OFF mimalloc=OFF\"\n"
    "    ARCHITECTURE \"x86_64\"\n"
    "    ENDIANNESS \"little\"\n"
    "    POINTER_WIDTH_BITS \"64\"\n"
    "    BYTECODE_FORMAT \"quickjs-ng-bytecode-v1\"\n"
    "    CAPABILITY_MANIFEST_SHA256 \"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\"\n"
    "    BUILD_TYPE \"Release\"\n"
    "    LTO \"OFF\"\n"
    "    ASAN \"OFF\"\n"
    "    UBSAN \"OFF\"\n"
    "    TSAN \"OFF\"\n"
    "    SANDBOX_PROFILE \"production\"\n"
    "    MIMALLOC \"OFF\"\n"
    "    HOST \"ON\"\n"
    "    WORKER \"OFF\"\n"
    "    COMPILER_ID \"TestCC\"\n"
    "    COMPILER_VERSION \"1.0\"\n"
    "    TARGET_TRIPLE \"x86_64-linux-gnu\")\n"
    "install(FILES marker.txt DESTINATION share)\n"
    "include(CPack)\n"
    "add_custom_target(verify_refresh ALL COMMAND \"\${CMAKE_COMMAND}\" -E true)\n"
)

file(MAKE_DIRECTORY "${repo}/cmake")
file(COPY_FILE "${CAPSID_SOURCE_DIR}/cmake/build_identity.h.in"
    "${repo}/cmake/build_identity.h.in")
file(WRITE "${repo}/CMakeLists.txt" "${mini_cmakelists}")
file(WRITE "${repo}/marker.txt" "commit-1\n")
run_checked_git(add CMakeLists.txt cmake/build_identity.h.in marker.txt)
run_checked_git(commit -m "initial commit")

execute_process(
    COMMAND "${GIT_EXE}" -C "${repo}" rev-parse HEAD
    OUTPUT_VARIABLE commit1
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE rc1)
if(NOT rc1 EQUAL 0 OR "${commit1}" STREQUAL "")
    message(FATAL_ERROR "failed to read commit1 SHA")
endif()

set(configure_cmd "${CMAKE_COMMAND}" -S "${repo}" -B "${build}" -G "${CAPSID_TEST_GENERATOR}")
if(CAPSID_TEST_MAKE_PROGRAM)
    list(APPEND configure_cmd "-DCMAKE_MAKE_PROGRAM=${CAPSID_TEST_MAKE_PROGRAM}")
endif()
execute_process(
    COMMAND ${configure_cmd}
    RESULT_VARIABLE cfg_rc
    OUTPUT_VARIABLE cfg_out
    ERROR_VARIABLE cfg_err)
if(NOT cfg_rc EQUAL 0)
    message(FATAL_ERROR "initial configure failed (${cfg_rc}):\n${cfg_out}\n${cfg_err}")
endif()

set(prov_file "${build}/generated/build-provenance-record.txt")
file(READ "${prov_file}" prov1)
if(NOT prov1 MATCHES "capsidCommit=${commit1}\n" OR NOT prov1 MATCHES "capsidTreeClean=true\n")
    message(FATAL_ERROR "initial provenance did not record clean commit1 ${commit1}:\n${prov1}")
endif()

# Advance git HEAD with a new commit without touching CMakeLists.txt.
# A plain incremental `cmake --build` must detect the git ref change via
# CMAKE_CONFIGURE_DEPENDS, re-run configure, and refresh the provenance record.
file(WRITE "${repo}/marker.txt" "commit-2\n")
run_checked_git(add marker.txt)
run_checked_git(commit -m "second commit")

execute_process(
    COMMAND "${GIT_EXE}" -C "${repo}" rev-parse HEAD
    OUTPUT_VARIABLE commit2
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE rc2)
if(NOT rc2 EQUAL 0 OR "${commit2}" STREQUAL "" OR "${commit2}" STREQUAL "${commit1}")
    message(FATAL_ERROR "failed to create distinct commit2 SHA")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${build}"
    RESULT_VARIABLE build_rc
    OUTPUT_VARIABLE build_out
    ERROR_VARIABLE build_err)
if(NOT build_rc EQUAL 0)
    message(FATAL_ERROR "incremental build after commit2 failed (${build_rc}):\n${build_out}\n${build_err}")
endif()

file(READ "${prov_file}" prov2)
if(NOT prov2 MATCHES "capsidCommit=${commit2}\n" OR NOT prov2 MATCHES "capsidTreeClean=true\n")
    message(FATAL_ERROR
        "incremental build left stale build identity (expected commit2=${commit2}):\n${prov2}")
endif()
if(prov2 STREQUAL prov1)
    message(FATAL_ERROR "provenance record did not change across commits")
endif()

function(check_install expected_error)
    execute_process(COMMAND "${CMAKE_COMMAND}" --install "${build}" --prefix "${run}/install"
        RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(expected_error STREQUAL "")
        if(NOT rc EQUAL 0)
            message(FATAL_ERROR "clean Release install failed: ${out}\n${err}")
        endif()
    elseif(rc EQUAL 0 OR NOT err MATCHES "${expected_error}")
        message(FATAL_ERROR "Release install expected '${expected_error}', got ${rc}: ${out}\n${err}")
    endif()
endfunction()

check_install("")
file(APPEND "${repo}/marker.txt" "unstaged edit\n")
check_install("clean worktree")
execute_process(COMMAND "${CMAKE_CPACK_COMMAND}" -G TGZ --config "${build}/CPackConfig.cmake"
    -B "${run}/packages" RESULT_VARIABLE pack_rc OUTPUT_VARIABLE pack_out ERROR_VARIABLE pack_err)
if(pack_rc EQUAL 0 OR NOT "${pack_out}\n${pack_err}" MATCHES "clean worktree")
    message(FATAL_ERROR "dirty direct CPack should be refused: ${pack_out}\n${pack_err}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${build}"
    RESULT_VARIABLE dirty_rc OUTPUT_VARIABLE dirty_out ERROR_VARIABLE dirty_err)
if(dirty_rc EQUAL 0 OR NOT "${dirty_out}\n${dirty_err}" MATCHES "clean worktree")
    message(FATAL_ERROR "dirty incremental Release build should be refused: ${dirty_out}\n${dirty_err}")
endif()
run_checked_git(add marker.txt)
run_checked_git(commit -m "third commit")
check_install("commit mismatch")
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${build}" COMMAND_ERROR_IS_FATAL ANY)
check_install("")

file(REMOVE_RECURSE "${run}")
message(STATUS "PASS: build identity refreshed automatically from ${commit1} to ${commit2}")
