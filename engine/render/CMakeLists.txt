add_library(ak_render STATIC
    src/Renderer.cpp
    src/RenderFoundation.cpp
    src/PrimitiveMesh.cpp
    src/MeshOptimization.cpp
    src/VulkanUploadPlan.cpp
    src/VulkanDrawSubmission.cpp
    src/VulkanRuntimeArchitecture.cpp
    src/VulkanFrameGraphCompiler.cpp
    src/VulkanFrameGraphExecution.cpp
)

add_library(AK::Render ALIAS ak_render)

target_include_directories(ak_render
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ak_render
    PUBLIC
        AK::Core
        AK::Platform
        AK::Math
        AK::World
        AK::RHI
)

set_target_properties(ak_render PROPERTIES FOLDER "Engine")
