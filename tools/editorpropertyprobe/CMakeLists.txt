add_executable(ak_editorpropertyprobe main.cpp)
target_link_libraries(ak_editorpropertyprobe PRIVATE AK::EditorUI)
set_target_properties(ak_editorpropertyprobe PROPERTIES FOLDER "Tools")
