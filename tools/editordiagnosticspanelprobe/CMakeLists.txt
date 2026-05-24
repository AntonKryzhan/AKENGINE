add_executable(ak_editordiagnosticspanelprobe
    main.cpp
)

target_link_libraries(ak_editordiagnosticspanelprobe
    PRIVATE
        AK::EditorUI
)

set_target_properties(ak_editordiagnosticspanelprobe PROPERTIES FOLDER "Tools")
