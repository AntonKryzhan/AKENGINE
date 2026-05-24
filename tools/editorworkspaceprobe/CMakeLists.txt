add_executable(ak_editorworkspaceprobe
    main.cpp
)

target_link_libraries(ak_editorworkspaceprobe
    PRIVATE
        AK::EditorUI
)

set_target_properties(ak_editorworkspaceprobe PROPERTIES FOLDER "Tools")
