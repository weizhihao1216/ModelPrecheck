add_library(FaultyWeapon SHARED faulty_weapon.cpp)
if(MSVC)
    add_compile_options(/utf-8)
endif()
