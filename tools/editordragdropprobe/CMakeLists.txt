add_executable(ak_editordragdropprobe
    src/main.cpp
)

target_link_libraries(ak_editordragdropprobe
    PRIVATE
        AK::EditorUI
)

set_target_properties(ak_editordragdropprobe PROPERTIES FOLDER "Tools")
