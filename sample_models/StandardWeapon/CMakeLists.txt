add_library(StandardWeapon SHARED standard_weapon.cpp)
if(MSVC)
    add_compile_options(/utf-8)
endif()
