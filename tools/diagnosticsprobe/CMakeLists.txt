add_executable(ak_diagnosticsprobe
    src/main.cpp
)

target_link_libraries(ak_diagnosticsprobe
    PRIVATE
        AK::Core
        AK::Diagnostics
)

set_target_properties(ak_diagnosticsprobe PROPERTIES FOLDER "Tools")
