add_executable(ak_pointcloudcookprobe
    src/main.cpp
)

target_link_libraries(ak_pointcloudcookprobe
    PRIVATE
        AK::PointCloud
)

set_target_properties(ak_pointcloudcookprobe PROPERTIES FOLDER "Tools")
