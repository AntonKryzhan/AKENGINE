add_executable(ak_editoruxprobe
    src/main.cpp
)

target_link_libraries(ak_editoruxprobe
    PRIVATE
        AK::EditorUI
)

set_target_properties(ak_editoruxprobe PROPERTIES FOLDER "Tools")
