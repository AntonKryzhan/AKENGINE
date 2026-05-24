add_executable(ak_editorruntimeprobe
    src/main.cpp
)

target_link_libraries(ak_editorruntimeprobe
    PRIVATE
        AK::EditorUI
)

set_target_properties(ak_editorruntimeprobe PROPERTIES FOLDER "Tools")
