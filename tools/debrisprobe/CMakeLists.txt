add_executable(ak_debrisprobe
    src/main.cpp
)

target_link_libraries(ak_debrisprobe
    PRIVATE
        AK::Core
        AK::Debris
)

set_target_properties(ak_debrisprobe PROPERTIES FOLDER "Tools")
