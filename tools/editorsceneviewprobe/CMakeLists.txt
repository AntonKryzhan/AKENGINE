add_executable(ak_editorsceneviewprobe
    main.cpp
)

target_link_libraries(ak_editorsceneviewprobe
    PRIVATE
        AK::EditorUI
)

set_target_properties(ak_editorsceneviewprobe PROPERTIES FOLDER "Tools")
