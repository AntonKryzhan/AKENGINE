add_library(ak_core STATIC
    src/Guid.cpp
    src/Log.cpp
    src/Path.cpp
    src/Time.cpp
)

add_library(AK::Core ALIAS ak_core)

target_include_directories(ak_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(ak_core PUBLIC cxx_std_20)

set_target_properties(ak_core PROPERTIES FOLDER "Engine")
