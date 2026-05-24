add_executable(ak_splineprobe
    src/main.cpp
)

target_link_libraries(ak_splineprobe
    PRIVATE
        AK::Core
        AK::Spline
)

set_target_properties(ak_splineprobe PROPERTIES FOLDER "Tools")
