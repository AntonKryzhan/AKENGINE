add_executable(ak_settingsprobe
    src/main.cpp
)

target_link_libraries(ak_settingsprobe
    PRIVATE
        AK::Core
        AK::Settings
)

set_target_properties(ak_settingsprobe PROPERTIES FOLDER "Tools")
