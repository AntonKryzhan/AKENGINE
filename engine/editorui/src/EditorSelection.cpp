#include <AK/EditorUI/EditorSelection.hpp>

#include <algorithm>
#include <sstream>
#include <unordered_set>

namespace AK
{
    namespace
    {
        bool SameSelectionId(const EditorSelectionItem& a, const EditorSelectionItem& b)
        {
            return a.domain == b.domain && a.stableId == b.stableId;
        }
    }

    const char* ToString(EditorSelectionDomain domain)
    {
        switch (domain)
        {
            case EditorSelectionDomain::None: return "none";
            case EditorSelectionDomain::SceneEntity: return "scene-entity";
            case EditorSelectionDomain::Asset: return "asset";
            case EditorSelectionDomain::Panel: return "panel";
            case EditorSelectionDomain::SubObject: return "sub-object";
            default: return "unknown";
        }
    }

    void ClearSelection(EditorSelectionModel& selection)
    {
        selection.items.clear();
        selection.active = {};
        ++selection.revision;
    }

    void SetSelection(EditorSelectionModel& selection, EditorSelectionItem item)
    {
        selection.items.clear();
        selection.items.push_back(item);
        selection.active = std::move(item);
        ++selection.revision;
    }

    void AddSelection(EditorSelectionModel& selection, EditorSelectionItem item)
    {
        const auto found = std::find_if(selection.items.begin(), selection.items.end(), [&](const EditorSelectionItem& existing)
        {
            return SameSelectionId(existing, item);
        });
        if (found == selection.items.end())
        {
            selection.items.push_back(item);
        }
        selection.active = std::move(item);
        ++selection.revision;
    }

    bool RemoveSelection(EditorSelectionModel& selection, const std::string& stableId)
    {
        const auto oldSize = selection.items.size();
        selection.items.erase(std::remove_if(selection.items.begin(), selection.items.end(), [&](const EditorSelectionItem& item)
        {
            return item.stableId == stableId;
        }), selection.items.end());

        if (selection.items.size() == oldSize)
        {
            return false;
        }

        if (selection.active.stableId == stableId)
        {
            selection.active = selection.items.empty() ? EditorSelectionItem{} : selection.items.front();
        }
        ++selection.revision;
        return true;
    }

    EditorSelectionSnapshot CaptureSelection(const EditorSelectionModel& selection)
    {
        EditorSelectionSnapshot snapshot{};
        snapshot.items = selection.items;
        snapshot.active = selection.active;
        snapshot.revision = selection.revision;
        return snapshot;
    }

    EditorSelectionDiagnostics ValidateEditorSelection(const EditorSelectionModel& selection)
    {
        EditorSelectionDiagnostics diagnostics{};
        diagnostics.selectedCount = selection.items.size();
        diagnostics.hasActive = !selection.active.stableId.empty();

        std::unordered_set<std::string> selectionIds;
        for (const EditorSelectionItem& item : selection.items)
        {
            std::string selectionId = ToString(item.domain);
            selectionId.push_back(':');
            selectionId.append(item.stableId);
            if (!selectionIds.insert(std::move(selectionId)).second)
            {
                ++diagnostics.duplicateCount;
            }
            if (diagnostics.hasActive && SameSelectionId(item, selection.active))
            {
                diagnostics.activeInSelection = true;
            }
        }

        diagnostics.ok = diagnostics.duplicateCount == 0 && (!diagnostics.hasActive || diagnostics.activeInSelection);

        std::ostringstream out;
        out << "selection count=" << diagnostics.selectedCount
            << " active=" << (diagnostics.hasActive ? "true" : "false")
            << " activeInSelection=" << (diagnostics.activeInSelection ? "true" : "false")
            << " duplicates=" << diagnostics.duplicateCount
            << " ok=" << (diagnostics.ok ? "true" : "false");
        diagnostics.summary = out.str();
        return diagnostics;
    }

    std::string FormatEditorSelectionItem(const EditorSelectionItem& item)
    {
        std::ostringstream out;
        out << ToString(item.domain) << ':' << item.stableId << " name=" << item.displayName;
        if (item.entity.IsValid())
        {
            out << " entity=" << ToString(item.entity);
        }
        return out.str();
    }
}
