#pragma once

#define AK_VERSION_MAJOR AK_ENGINE_VERSION_MAJOR
#define AK_VERSION_MINOR AK_ENGINE_VERSION_MINOR
#define AK_VERSION_PATCH AK_ENGINE_VERSION_PATCH

namespace AK
{
    constexpr int VersionMajor = AK_VERSION_MAJOR;
    constexpr int VersionMinor = AK_VERSION_MINOR;
    constexpr int VersionPatch = AK_VERSION_PATCH;
}
