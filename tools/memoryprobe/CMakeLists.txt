add_executable(ak_memoryprobe
    src/main.cpp
)

target_link_libraries(ak_memoryprobe
    PRIVATE
        AK::Core
        AK::Memory
)

set_target_properties(ak_memoryprobe PROPERTIES FOLDER "Tools")
