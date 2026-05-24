add_executable(ak_editorautosaveprobe
    main.cpp
)

target_link_libraries(ak_editorautosaveprobe
    PRIVATE
        AK::EditorUI
)

set_target_properties(ak_editorautosaveprobe PROPERTIES FOLDER "Tools")
