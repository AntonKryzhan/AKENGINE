#include <AK/EditorUI/EditorSearch.hpp>

#include <algorithm>
#include <cctype>
#include <sstream>
#include <unordered_set>

namespace AK
{
    namespace
    {
        std::string Lower(std::string text)
        {
            std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            });
            return text;
        }

        std::string NormalizeQuery(std::string text)
        {
            const auto begin = std::find_if_not(text.begin(), text.end(), [](unsigned char c)
            {
                return std::isspace(c) != 0;
            });
            const auto end = std::find_if_not(text.rbegin(), text.rend(), [](unsigned char c)
            {
                return std::isspace(c) != 0;
            }).base();

            if (begin >= end)
            {
                text.clear();
            }
            else
            {
                text.assign(begin, end);
            }
            return Lower(text);
        }

        float ScoreText(const std::string& query, const std::string& title, const std::string& subtitle)
        {
            const std::string q = NormalizeQuery(query);
            if (q.empty())
            {
                return 0.25f;
            }

            const std::string t = Lower(title);
            const std::string s = Lower(subtitle);
            if (t == q)
            {
                return 1.0f;
            }
            if (t.find(q) == 0)
            {
                return 0.92f;
            }
            if (t.find(q) != std::string::npos)
            {
                return 0.78f;
            }
            if (s.find(q) != std::string::npos)
            {
                return 0.55f;
            }
            return 0.0f;
        }

        void PushCommandRecords(EditorSearchIndex& index, const CommandRegistry& commands)
        {
            for (const CommandDescriptor& command : commands.Commands())
            {
                EditorSearchResult result{};
                result.kind = EditorSearchResultKind::Command;
                result.title = command.displayName;
                result.subtitle = std::string(command.category) + " / " + command.description;
                result.stableId = std::string("command:") + command.name;
                result.command = command.id;
                result.score = 0.0f;
                index.records.push_back(std::move(result));
            }
        }

        void PushPanelRecords(EditorSearchIndex& index, const EditorPanelRegistry& panels)
        {
            for (const EditorPanelDesc& panel : panels.Panels())
            {
                if (!HasCapability(panel.capabilities, EditorPanelCapability::Searchable))
                {
                    continue;
                }
                EditorSearchResult result{};
                result.kind = EditorSearchResultKind::Panel;
                result.title = panel.title;
                result.subtitle = std::string("Panel / ") + panel.category + " / " + ToString(panel.kind);
                result.stableId = std::string("panel:") + panel.name;
                result.panel = panel.id;
                index.records.push_back(std::move(result));
            }
        }

        void PushEntityRecords(EditorSearchIndex& index, const std::vector<EditorSelectionItem>& sceneEntities)
        {
            for (const EditorSelectionItem& entity : sceneEntities)
            {
                EditorSearchResult result{};
                result.kind = EditorSearchResultKind::Entity;
                result.title = entity.displayName;
                result.subtitle = "Scene Entity";
                result.stableId = std::string("entity:") + entity.stableId;
                result.entity = entity.entity;
                index.records.push_back(std::move(result));
            }
        }

        void PushAssetRecords(EditorSearchIndex& index, const std::vector<std::string>& assetNames)
        {
            for (const std::string& assetName : assetNames)
            {
                EditorSearchResult result{};
                result.kind = EditorSearchResultKind::Asset;
                result.title = assetName;
                result.subtitle = "Asset Database";
                result.stableId = std::string("asset:") + assetName;
                index.records.push_back(std::move(result));
            }
        }
    }

    const char* ToString(EditorSearchResultKind kind)
    {
        switch (kind)
        {
            case EditorSearchResultKind::Command: return "command";
            case EditorSearchResultKind::Panel: return "panel";
            case EditorSearchResultKind::Asset: return "asset";
            case EditorSearchResultKind::Entity: return "entity";
            case EditorSearchResultKind::Setting: return "setting";
            case EditorSearchResultKind::Overlay: return "overlay";
            default: return "unknown";
        }
    }

    EditorSearchIndex BuildDefaultEditorSearchIndex(const CommandRegistry& commands, const EditorPanelRegistry& panels, const std::vector<EditorSelectionItem>& sceneEntities, const std::vector<std::string>& assetNames)
    {
        EditorSearchIndex index{};
        PushCommandRecords(index, commands);
        PushPanelRecords(index, panels);
        PushEntityRecords(index, sceneEntities);
        PushAssetRecords(index, assetNames);
        return index;
    }

    std::vector<EditorSearchResult> SearchEditorIndex(const EditorSearchIndex& index, const EditorSearchQuery& query)
    {
        std::vector<EditorSearchResult> results;
        results.reserve(std::min(query.maxResults, index.records.size()));
        for (EditorSearchResult record : index.records)
        {
            record.score = ScoreText(query.text, record.title, record.subtitle);
            if (record.score > 0.0f || (query.includeHidden && query.text.empty()))
            {
                results.push_back(std::move(record));
            }
        }

        std::sort(results.begin(), results.end(), [](const EditorSearchResult& a, const EditorSearchResult& b)
        {
            if (a.score != b.score)
            {
                return a.score > b.score;
            }
            if (a.kind != b.kind)
            {
                return static_cast<int>(a.kind) < static_cast<int>(b.kind);
            }
            return a.title < b.title;
        });

        if (results.size() > query.maxResults)
        {
            results.resize(query.maxResults);
        }
        return results;
    }

    EditorSearchDiagnostics ValidateEditorSearchIndex(const EditorSearchIndex& index)
    {
        EditorSearchDiagnostics diagnostics{};
        diagnostics.indexedRecordCount = index.records.size();
        std::unordered_set<std::string> stableIds;
        for (const EditorSearchResult& record : index.records)
        {
            if (!stableIds.insert(record.stableId).second)
            {
                ++diagnostics.duplicateStableIdCount;
            }
            switch (record.kind)
            {
                case EditorSearchResultKind::Command: ++diagnostics.commandRecordCount; break;
                case EditorSearchResultKind::Panel: ++diagnostics.panelRecordCount; break;
                case EditorSearchResultKind::Asset: ++diagnostics.assetRecordCount; break;
                case EditorSearchResultKind::Entity: ++diagnostics.entityRecordCount; break;
                default: break;
            }
        }

        diagnostics.ok = diagnostics.indexedRecordCount >= 20 && diagnostics.commandRecordCount > 0 && diagnostics.panelRecordCount > 0 && diagnostics.duplicateStableIdCount == 0;

        std::ostringstream out;
        out << "search records=" << diagnostics.indexedRecordCount
            << " commands=" << diagnostics.commandRecordCount
            << " panels=" << diagnostics.panelRecordCount
            << " assets=" << diagnostics.assetRecordCount
            << " entities=" << diagnostics.entityRecordCount
            << " duplicates=" << diagnostics.duplicateStableIdCount
            << " ok=" << (diagnostics.ok ? "true" : "false");
        diagnostics.summary = out.str();
        return diagnostics;
    }

    std::string FormatEditorSearchResult(const EditorSearchResult& result)
    {
        std::ostringstream out;
        out << ToString(result.kind) << ':' << result.title << " score=" << result.score << " id=" << result.stableId;
        return out.str();
    }
}
