add_library(ConflictWeaponA SHARED conflict_weapon_a.cpp)
target_compile_definitions(ConflictWeaponA PRIVATE WEAPON_EXPORTS)
if(MSVC)
    target_compile_options(ConflictWeaponA PRIVATE /utf-8)
endif()
add_custom_command(TARGET ConflictWeaponA POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/WeaponModel.h"
        "$<TARGET_FILE_DIR:ConflictWeaponA>/WeaponModel.h"
    COMMENT "Copy ConflictWeaponA header beside DLL")
