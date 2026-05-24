add_executable(ak_editorcommandpaletteruntimeprobe
    src/main.cpp
)

target_link_libraries(ak_editorcommandpaletteruntimeprobe
    PRIVATE
        AK::EditorUI
)

set_target_properties(ak_editorcommandpaletteruntimeprobe PROPERTIES FOLDER "Tools")
