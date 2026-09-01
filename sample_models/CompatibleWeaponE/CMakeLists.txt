add_library(CompatibleWeaponE SHARED compatible_weapon_e.cpp)
target_compile_definitions(CompatibleWeaponE PRIVATE WEAPON_EXPORTS)
if(MSVC)
    target_compile_options(CompatibleWeaponE PRIVATE /utf-8)
endif()
add_custom_command(TARGET CompatibleWeaponE POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/WeaponModel.h"
        "$<TARGET_FILE_DIR:CompatibleWeaponE>/WeaponModel.h"
    COMMENT "Copy CompatibleWeaponE header beside DLL")
