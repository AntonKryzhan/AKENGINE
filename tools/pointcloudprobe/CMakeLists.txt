add_executable(ak_pointcloudprobe
    src/main.cpp
)

target_link_libraries(ak_pointcloudprobe
    PRIVATE
        AK::PointCloud
)

set_target_properties(ak_pointcloudprobe PROPERTIES FOLDER "Tools")
