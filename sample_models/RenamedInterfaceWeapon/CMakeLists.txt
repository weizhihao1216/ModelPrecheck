add_library(RenamedInterfaceWeapon SHARED renamed_weapon.cpp)
target_compile_definitions(RenamedInterfaceWeapon PRIVATE WEAPON_EXPORTS)
if(MSVC)
    target_compile_options(RenamedInterfaceWeapon PRIVATE /utf-8)
endif()
add_custom_command(TARGET RenamedInterfaceWeapon POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/WeaponModel.h"
        "$<TARGET_FILE_DIR:RenamedInterfaceWeapon>/WeaponModel.h"
    COMMENT "Copy RenamedInterfaceWeapon header beside DLL")
