add_library(ak_rhi STATIC
    src/VulkanRHI.cpp
)

add_library(AK::RHI ALIAS ak_rhi)

target_include_directories(ak_rhi
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ak_rhi
    PUBLIC
        AK::Core
    PRIVATE
        ${CMAKE_DL_LIBS}
)

set_target_properties(ak_rhi PROPERTIES FOLDER "Engine")
