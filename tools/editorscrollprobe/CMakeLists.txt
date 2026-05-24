add_executable(ak_editorscrollprobe
    main.cpp
)

target_link_libraries(ak_editorscrollprobe
    PRIVATE
        AK::EditorUI
)

set_target_properties(ak_editorscrollprobe PROPERTIES FOLDER "Tools")
