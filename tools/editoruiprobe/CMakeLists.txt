add_executable(ak_editoruiprobe
    src/main.cpp
)

target_link_libraries(ak_editoruiprobe
    PRIVATE
        AK::EditorUI
)

set_target_properties(ak_editoruiprobe PROPERTIES FOLDER "Tools")
