add_executable(ak_editorcommandpaletteprobe
    main.cpp
)

target_link_libraries(ak_editorcommandpaletteprobe
    PRIVATE
        AK::EditorUI
)

set_target_properties(ak_editorcommandpaletteprobe PROPERTIES FOLDER "Tools")
