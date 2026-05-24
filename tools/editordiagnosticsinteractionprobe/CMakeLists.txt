add_executable(ak_editordiagnosticsinteractionprobe
    main.cpp
)

target_link_libraries(ak_editordiagnosticsinteractionprobe
    PRIVATE
        AK::EditorUI
)

set_target_properties(ak_editordiagnosticsinteractionprobe PROPERTIES FOLDER "Tools")
