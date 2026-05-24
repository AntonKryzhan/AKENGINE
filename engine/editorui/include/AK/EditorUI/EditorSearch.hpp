#pragma once

#include <AK/Commands/CommandRegistry.hpp>
#include <AK/EditorUI/EditorChrome.hpp>
#include <AK/EditorUI/EditorSelection.hpp>

#include <string>
#include <vector>

namespace AK
{
    enum class EditorSearchResultKind
    {
        Command,
        Panel,
        Asset,
        Entity,
        Setting,
        Overlay
    };

    struct EditorSearchQuery
    {
        std::string text;
        std::size_t maxResults = 32;
        bool includeHidden = false;
    };

    struct EditorSearchResult
    {
        EditorSearchResultKind kind = EditorSearchResultKind::Command;
        std::string title;
        std::string subtitle;
        std::string stableId;
        float score = 0.0f;
        CommandId command = CommandId::CloseEditor;
        EditorPanelId panel{};
        EntityId entity{};
    };

    struct EditorSearchIndex
    {
        std::vector<EditorSearchResult> records;
    };

    struct EditorSearchDiagnostics
    {
        std::size_t indexedRecordCount = 0;
        std::size_t commandRecordCount = 0;
        std::size_t panelRecordCount = 0;
        std::size_t assetRecordCount = 0;
        std::size_t entityRecordCount = 0;
        std::size_t duplicateStableIdCount = 0;
        bool ok = false;
        std::string summary;
    };

    const char* ToString(EditorSearchResultKind kind);
    EditorSearchIndex BuildDefaultEditorSearchIndex(const CommandRegistry& commands, const EditorPanelRegistry& panels, const std::vector<EditorSelectionItem>& sceneEntities, const std::vector<std::string>& assetNames);
    std::vector<EditorSearchResult> SearchEditorIndex(const EditorSearchIndex& index, const EditorSearchQuery& query);
    EditorSearchDiagnostics ValidateEditorSearchIndex(const EditorSearchIndex& index);
    std::string FormatEditorSearchResult(const EditorSearchResult& result);
}
