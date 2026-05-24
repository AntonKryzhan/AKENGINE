add_executable(ak_physicsprobe
    src/main.cpp
)

target_link_libraries(ak_physicsprobe
    PRIVATE
        AK::Core
        AK::Physics
)

set_target_properties(ak_physicsprobe PROPERTIES FOLDER "Tools")
