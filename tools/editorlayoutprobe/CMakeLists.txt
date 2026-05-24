add_executable(ak_editorlayoutprobe
    main.cpp
)

target_link_libraries(ak_editorlayoutprobe
    PRIVATE
        AK::EditorUI
)

set_target_properties(ak_editorlayoutprobe PROPERTIES FOLDER "Tools")
