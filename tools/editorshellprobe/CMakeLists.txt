add_executable(ak_editorshellprobe
    main.cpp
)

target_link_libraries(ak_editorshellprobe
    PRIVATE
        AK::Core
        AK::Commands
        AK::EditorUI
        AK::Render
)

set_target_properties(ak_editorshellprobe PROPERTIES FOLDER "Tools")
