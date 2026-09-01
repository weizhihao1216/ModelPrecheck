add_library(CompatibleWeaponC SHARED compatible_weapon_c.cpp)
target_compile_definitions(CompatibleWeaponC PRIVATE WEAPON_EXPORTS)
if(MSVC)
    target_compile_options(CompatibleWeaponC PRIVATE /utf-8)
endif()
add_custom_command(TARGET CompatibleWeaponC POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/WeaponModel.h"
        "$<TARGET_FILE_DIR:CompatibleWeaponC>/WeaponModel.h"
    COMMENT "Copy CompatibleWeaponC header beside DLL")
