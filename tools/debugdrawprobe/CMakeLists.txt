add_executable(ak_debugdrawprobe
    src/main.cpp
)

target_link_libraries(ak_debugdrawprobe
    PRIVATE
        AK::DebugDraw
)

set_target_properties(ak_debugdrawprobe PROPERTIES FOLDER "Tools")
