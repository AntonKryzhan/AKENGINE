add_executable(ak_serializationprobe
    src/main.cpp
)

target_link_libraries(ak_serializationprobe
    PRIVATE
        AK::Core
        AK::Serialization
)

set_target_properties(ak_serializationprobe PROPERTIES FOLDER "Tools")
