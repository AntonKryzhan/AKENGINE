add_library(ak_policy STATIC
    src/EnginePolicies.cpp
)

add_library(AK::Policy ALIAS ak_policy)

target_include_directories(ak_policy
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ak_policy
    PUBLIC
        AK::Core
)

target_compile_definitions(ak_policy
    PUBLIC
        $<$<CONFIG:Debug>:AK_BUILD_MODE_DEBUG=1>
        $<$<CONFIG:Development>:AK_BUILD_MODE_DEVELOPMENT=1>
        $<$<CONFIG:RelWithDebInfo>:AK_BUILD_MODE_DEVELOPMENT=1>
        $<$<CONFIG:Release>:AK_BUILD_MODE_RELEASE=1>
        $<$<CONFIG:MinSizeRel>:AK_BUILD_MODE_SHIPPING=1>
)

set_target_properties(ak_policy PROPERTIES FOLDER "Engine")
