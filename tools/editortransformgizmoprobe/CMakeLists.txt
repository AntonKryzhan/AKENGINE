add_executable(ak_editortransformgizmoprobe
    main.cpp
)

target_link_libraries(ak_editortransformgizmoprobe
    PRIVATE
        AK::EditorUI
)

set_target_properties(ak_editortransformgizmoprobe PROPERTIES FOLDER "Tools")
