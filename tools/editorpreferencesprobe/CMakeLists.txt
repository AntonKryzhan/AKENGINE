add_executable(ak_editorpreferencesprobe
    main.cpp
)

target_link_libraries(ak_editorpreferencesprobe
    PRIVATE
        AK::EditorUI
)

set_target_properties(ak_editorpreferencesprobe PROPERTIES FOLDER "Tools")
