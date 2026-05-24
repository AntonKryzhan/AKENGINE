add_executable(ak_editoruxruntimeprobe
    main.cpp
)

target_link_libraries(ak_editoruxruntimeprobe
    PRIVATE
        AK::EditorUI
)

set_target_properties(ak_editoruxruntimeprobe PROPERTIES FOLDER "Tools")
