#pragma once

#include <cstdlib>
#include <iostream>

#define AK_ASSERT(condition, message) \
    do \
    { \
        if (!(condition)) \
        { \
            std::cerr << "AK_ASSERT failed: " << (message) << "\n"; \
            std::abort(); \
        } \
    } while (false)
