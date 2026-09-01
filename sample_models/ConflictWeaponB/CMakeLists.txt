add_library(ConflictWeaponB SHARED conflict_weapon_b.cpp)
target_compile_definitions(ConflictWeaponB PRIVATE WEAPON_EXPORTS)
if(MSVC)
    target_compile_options(ConflictWeaponB PRIVATE /utf-8)
endif()
add_custom_command(TARGET ConflictWeaponB POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/WeaponModel.h"
        "$<TARGET_FILE_DIR:ConflictWeaponB>/WeaponModel.h"
    COMMENT "Copy ConflictWeaponB header beside DLL")
