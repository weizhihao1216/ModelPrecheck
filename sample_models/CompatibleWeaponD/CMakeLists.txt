add_library(CompatibleWeaponD SHARED compatible_weapon_d.cpp)
target_compile_definitions(CompatibleWeaponD PRIVATE WEAPON_EXPORTS)
if(MSVC)
    target_compile_options(CompatibleWeaponD PRIVATE /utf-8)
endif()
add_custom_command(TARGET CompatibleWeaponD POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/WeaponModel.h"
        "$<TARGET_FILE_DIR:CompatibleWeaponD>/WeaponModel.h"
    COMMENT "Copy CompatibleWeaponD header beside DLL")
