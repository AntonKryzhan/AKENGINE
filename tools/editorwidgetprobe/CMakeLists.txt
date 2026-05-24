add_executable(ak_editorwidgetprobe
    src/main.cpp
)

target_link_libraries(ak_editorwidgetprobe
    PRIVATE
        AK::EditorUI
)

set_target_properties(ak_editorwidgetprobe PROPERTIES FOLDER "Tools")
