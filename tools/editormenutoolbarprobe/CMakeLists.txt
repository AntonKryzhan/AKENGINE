add_executable(ak_editormenutoolbarprobe
    main.cpp
)

target_link_libraries(ak_editormenutoolbarprobe
    PRIVATE
        AK::EditorUI
)

set_target_properties(ak_editormenutoolbarprobe PROPERTIES FOLDER "Tools")
