#include "capsid/runtime.h"

#include "build_identity.h"

capsid_result capsid_runtime_build_info(capsid_build_info* out_info) {
    if (!out_info || out_info->struct_size != sizeof(*out_info) ||
        out_info->version != CAPSID_BUILD_INFO_VERSION) {
        return CAPSID_INVALID_ARGUMENT;
    }
    out_info->runtime_version = CAPSID_BUILD_RUNTIME_VERSION;
    out_info->abi_version = CAPSID_BUILD_ABI_VERSION;
    out_info->fetchrpc_version = CAPSID_BUILD_FETCHRPC_VERSION;
    out_info->quickjs_commit = CAPSID_BUILD_QUICKJS_COMMIT;
    out_info->txiki_overlay_key = CAPSID_BUILD_TXIKI_OVERLAY_KEY;
    out_info->txiki_overlay_manifest = CAPSID_BUILD_TXIKI_OVERLAY_MANIFEST;
    out_info->bytecode_compile_flags = CAPSID_BUILD_BYTECODE_COMPILE_FLAGS;
    out_info->target_architecture = CAPSID_BUILD_TARGET_ARCHITECTURE;
    out_info->endianness = CAPSID_BUILD_ENDIANNESS;
    out_info->pointer_width_bits = CAPSID_BUILD_POINTER_WIDTH_BITS;
    out_info->bytecode_format_identity = CAPSID_BUILD_BYTECODE_FORMAT_IDENTITY;
    out_info->capability_manifest_sha256 = CAPSID_BUILD_CAPABILITY_MANIFEST_SHA256;
    out_info->compatibility_id = CAPSID_BUILD_COMPATIBILITY_ID;
    out_info->build_id = CAPSID_BUILD_BUILD_ID;
    out_info->capsid_commit = CAPSID_BUILD_CAPSID_COMMIT;
    out_info->capsid_tree_clean = CAPSID_BUILD_TREE_CLEAN;
    out_info->provenance_dirty = CAPSID_BUILD_PROVENANCE_DIRTY;
    out_info->compiler_id = CAPSID_BUILD_COMPILER_ID;
    out_info->compiler_version = CAPSID_BUILD_COMPILER_VERSION;
    out_info->target_triple = CAPSID_BUILD_TARGET_TRIPLE;
    out_info->cmake_build_type = CAPSID_BUILD_TYPE;
    out_info->build_feature_flags = CAPSID_BUILD_FEATURE_FLAGS;
    return CAPSID_OK;
}
