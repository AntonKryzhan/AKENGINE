add_executable(ak_editorfocusprobe main.cpp)
target_link_libraries(ak_editorfocusprobe PRIVATE AK::EditorUI)
set_target_properties(ak_editorfocusprobe PROPERTIES FOLDER "Tools")
