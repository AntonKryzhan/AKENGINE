add_library(ak_jobs
    include/AK/Jobs/JobSystem.hpp
    src/JobSystem.cpp
)

add_library(AK::Jobs ALIAS ak_jobs)

target_include_directories(ak_jobs
    PUBLIC
        include
)

target_link_libraries(ak_jobs
    PUBLIC
        AK::Core
)

set_target_properties(ak_jobs PROPERTIES FOLDER "Engine")
