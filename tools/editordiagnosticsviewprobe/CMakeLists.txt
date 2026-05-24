add_executable(ak_editordiagnosticsviewprobe
    main.cpp
)

target_link_libraries(ak_editordiagnosticsviewprobe
    PRIVATE
        AK::EditorUI
)

set_target_properties(ak_editordiagnosticsviewprobe PROPERTIES FOLDER "Tools")
