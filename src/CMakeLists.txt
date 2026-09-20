# We link everything statically, so there are no runtime DLLs to copy beside the
# plugin. vcpkg's applocal step looks for an installed bin/ directory that a
# static triplet never creates, fails, and aborts the rest of the post-build --
# including the deploy below, which silently leaves a stale DLL in the mod.
set(VCPKG_APPLOCAL_DEPS OFF)

add_commonlibsse_plugin(ModernCameraCollision
    SOURCES
        Plugin.cpp
        Settings.cpp
        Collision.cpp
        Whiskers.cpp
        Motion.cpp
        Fade.cpp
        Draw.cpp
    NAME "ModernCameraCollision"
    AUTHOR "Sami"
    VERSION 0.1.0
    USE_ADDRESS_LIBRARY
)

target_include_directories(ModernCameraCollision PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_precompile_headers(ModernCameraCollision PRIVATE PCH.h)
target_compile_definitions(ModernCameraCollision PRIVATE MCC_VERSION="${PROJECT_VERSION}")

# A PDB for Release too: crash logs are symbolised against it.
target_compile_options(ModernCameraCollision PRIVATE "$<$<CONFIG:Release>:/Zi>")
target_link_options(ModernCameraCollision PRIVATE "$<$<CONFIG:Release>:/DEBUG;/OPT:REF;/OPT:ICF>")

# MinHook: hooking a function entry needs its prologue relocated, which the
# SKSE trampoline's write_branch does not do.
find_package(minhook CONFIG REQUIRED)

target_link_libraries(ModernCameraCollision PRIVATE
    minhook::minhook
    d3d11.lib
    dxgi.lib
    d3dcompiler.lib)

# Deploy as an ordinary MO2 mod so it can be toggled in the load order and
# removed without residue.
set(MCC_DEPLOY_DIR
    "$ENV{LOCALAPPDATA}/ModOrganizer/Skyrim Special Edition/mods/ModernCameraCollision"
    CACHE PATH "Mod folder the built DLL is copied into")

if(MCC_DEPLOY_DIR)
    add_custom_command(TARGET ModernCameraCollision POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${MCC_DEPLOY_DIR}/SKSE/Plugins"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:ModernCameraCollision>" "${MCC_DEPLOY_DIR}/SKSE/Plugins/"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_PDB_FILE:ModernCameraCollision>" "${MCC_DEPLOY_DIR}/SKSE/Plugins/"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
                "${CMAKE_SOURCE_DIR}/dist" "${MCC_DEPLOY_DIR}"
        COMMENT "Deploying ModernCameraCollision.dll to ${MCC_DEPLOY_DIR}")
endif()
