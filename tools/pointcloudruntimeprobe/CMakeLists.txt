add_executable(ak_pointcloudruntimeprobe
    src/main.cpp
)

target_link_libraries(ak_pointcloudruntimeprobe
    PRIVATE
        AK::PointCloud
)

set_target_properties(ak_pointcloudruntimeprobe PROPERTIES FOLDER "Tools")
