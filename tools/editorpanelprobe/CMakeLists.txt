add_executable(ak_editorpanelprobe
    src/main.cpp
)

target_link_libraries(ak_editorpanelprobe
    PRIVATE
        AK::EditorUI
)

set_target_properties(ak_editorpanelprobe PROPERTIES FOLDER "Tools")
