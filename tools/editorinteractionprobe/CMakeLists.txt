add_executable(ak_editorinteractionprobe
    src/main.cpp
)

target_link_libraries(ak_editorinteractionprobe
    PRIVATE
        AK::EditorUI
)

set_target_properties(ak_editorinteractionprobe PROPERTIES FOLDER "Tools")
