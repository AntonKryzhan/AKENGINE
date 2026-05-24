add_executable(ak_editorshortcutprobe
    main.cpp
)

target_link_libraries(ak_editorshortcutprobe
    PRIVATE
        AK::EditorUI
)

set_target_properties(ak_editorshortcutprobe PROPERTIES FOLDER "Tools")
