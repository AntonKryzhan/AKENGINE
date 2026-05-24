add_executable(ak_editoractivityprobe
    main.cpp
)

target_link_libraries(ak_editoractivityprobe
    PRIVATE
        AK::EditorUI
)

set_target_properties(ak_editoractivityprobe PROPERTIES FOLDER "Tools")
