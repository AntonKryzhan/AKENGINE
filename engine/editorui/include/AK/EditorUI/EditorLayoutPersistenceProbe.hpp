#pragma once

#include <AK/EditorUI/EditorLayoutPersistence.hpp>

#include <string>

namespace AK
{
    struct EditorLayoutPersistenceProbeResult
    {
        EditorLayoutLoadResult cleanLoad{};
        EditorLayoutRepairReport corruptedRepair{};
        EditorLayoutLoadResult legacyLoad{};
        EditorWindowPlacement clampedPlacement{};
        std::size_t issueCount = 0;
        bool duplicateRemoved = false;
        bool unknownRemoved = false;
        bool activeTabClamped = false;
        bool splitWeightClamped = false;
        bool windowClamped = false;
        bool resetFallback = false;
        bool ok = false;
        std::string summary;
    };

    EditorLayoutPersistenceProbeResult BuildEditorLayoutPersistenceProbe();
}
