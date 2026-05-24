add_executable(ak_editorsessionprobe
    main.cpp
)

target_link_libraries(ak_editorsessionprobe
    PRIVATE
        AK::EditorUI
)

set_target_properties(ak_editorsessionprobe PROPERTIES FOLDER "Tools")
