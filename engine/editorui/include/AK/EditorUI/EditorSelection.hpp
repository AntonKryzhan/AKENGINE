#pragma once

#include <AK/Core/Types.hpp>
#include <AK/ECS/EntityId.hpp>

#include <string>
#include <vector>

namespace AK
{
    enum class EditorSelectionDomain
    {
        None,
        SceneEntity,
        Asset,
        Panel,
        SubObject
    };

    struct EditorSelectionItem
    {
        EditorSelectionDomain domain = EditorSelectionDomain::None;
        EntityId entity{};
        std::string stableId;
        std::string displayName;
    };

    struct EditorSelectionSnapshot
    {
        std::vector<EditorSelectionItem> items;
        EditorSelectionItem active{};
        u64 revision = 0;
    };

    struct EditorSelectionModel
    {
        std::vector<EditorSelectionItem> items;
        EditorSelectionItem active{};
        u64 revision = 1;
    };

    struct EditorSelectionDiagnostics
    {
        std::size_t selectedCount = 0;
        std::size_t duplicateCount = 0;
        bool hasActive = false;
        bool activeInSelection = false;
        bool ok = false;
        std::string summary;
    };

    const char* ToString(EditorSelectionDomain domain);
    void ClearSelection(EditorSelectionModel& selection);
    void SetSelection(EditorSelectionModel& selection, EditorSelectionItem item);
    void AddSelection(EditorSelectionModel& selection, EditorSelectionItem item);
    bool RemoveSelection(EditorSelectionModel& selection, const std::string& stableId);
    EditorSelectionSnapshot CaptureSelection(const EditorSelectionModel& selection);
    EditorSelectionDiagnostics ValidateEditorSelection(const EditorSelectionModel& selection);
    std::string FormatEditorSelectionItem(const EditorSelectionItem& item);
}
