add_executable(ak_editortextprobe
    main.cpp
)

target_link_libraries(ak_editortextprobe
    PRIVATE
        AK::EditorUI
)

set_target_properties(ak_editortextprobe PROPERTIES FOLDER "Tools")
