add_executable(ak_editorthemeprobe
    main.cpp
)

target_link_libraries(ak_editorthemeprobe
    PRIVATE
        AK::EditorUI
)

set_target_properties(ak_editorthemeprobe PROPERTIES FOLDER "Tools")
