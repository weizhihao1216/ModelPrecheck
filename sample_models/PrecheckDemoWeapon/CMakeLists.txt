add_library(PrecheckDemoWeapon SHARED precheck_demo_weapon.cpp)
target_compile_definitions(PrecheckDemoWeapon PRIVATE PRECHECK_DEMO_EXPORTS)
if(MSVC)
    target_compile_options(PrecheckDemoWeapon PRIVATE /utf-8)
endif()

add_custom_command(TARGET PrecheckDemoWeapon POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/WeaponModel.h"
        "$<TARGET_FILE_DIR:PrecheckDemoWeapon>/WeaponModel.h"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/WeaponObject.h"
        "$<TARGET_FILE_DIR:PrecheckDemoWeapon>/WeaponObject.h"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
        "$<TARGET_FILE_DIR:PrecheckDemoWeapon>/README.md"
    COMMENT "Copy PrecheckDemoWeapon package files beside DLL")
