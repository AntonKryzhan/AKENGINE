add_library(ak_pointcloud STATIC
    src/PointCloud.cpp
    src/PointCloudCook.cpp
    src/PointCloudRuntime.cpp
)

add_library(AK::PointCloud ALIAS ak_pointcloud)

target_include_directories(ak_pointcloud
    PUBLIC
        include
)

target_link_libraries(ak_pointcloud
    PUBLIC
        AK::Core
        AK::Math
        AK::GeoData
        AK::OutOfCore
)

set_target_properties(ak_pointcloud PROPERTIES FOLDER "Engine")
