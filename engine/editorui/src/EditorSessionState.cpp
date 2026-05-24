#include <AK/EditorUI/EditorSessionState.hpp>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_set>

namespace AK
{
    namespace
    {
        void AddIssue(EditorSessionRepairReport& report, EditorSessionRepairCode code, EditorSessionRepairSeverity severity, std::string message, EditorPanelId panel = {}, std::string stableId = {})
        {
            EditorSessionRepairIssue issue{};
            issue.code = code;
            issue.severity = severity;
            issue.panel = panel;
            issue.stableId = std::move(stableId);
            issue.message = std::move(message);
            report.issues.push_back(std::move(issue));
            if (code != EditorSessionRepairCode::None)
            {
                report.repaired = true;
            }
        }

        bool IsKnownStableId(const std::vector<std::string>& validStableIds, const std::string& stableId)
        {
            if (validStableIds.empty())
            {
                return true;
            }
            return std::find(validStableIds.begin(), validStableIds.end(), stableId) != validStableIds.end();
        }

        std::string SelectionKey(const EditorSessionSelectionEntry& item)
        {
            return std::to_string(static_cast<int>(item.domain)) + ":" + item.stableId;
        }

        EditorSelectionDomain SelectionDomainFromInt(int value)
        {
            switch (value)
            {
                case 1: return EditorSelectionDomain::SceneEntity;
                case 2: return EditorSelectionDomain::Asset;
                case 3: return EditorSelectionDomain::Panel;
                case 4: return EditorSelectionDomain::SubObject;
                default: return EditorSelectionDomain::None;
            }
        }

        EditorScrollPanelKind PanelNameToScrollKind(const std::string& name)
        {
            if (name == "scene.hierarchy") return EditorScrollPanelKind::Hierarchy;
            if (name == "inspector") return EditorScrollPanelKind::Inspector;
            if (name == "assets.browser") return EditorScrollPanelKind::ProjectGrid;
            if (name == "console") return EditorScrollPanelKind::Console;
            if (name == "diagnostics") return EditorScrollPanelKind::Diagnostics;
            return EditorScrollPanelKind::Unknown;
        }

        const char* ScrollKindToPanelName(EditorScrollPanelKind kind)
        {
            switch (kind)
            {
                case EditorScrollPanelKind::Hierarchy: return "scene.hierarchy";
                case EditorScrollPanelKind::Inspector: return "inspector";
                case EditorScrollPanelKind::ProjectTree: return "assets.browser";
                case EditorScrollPanelKind::ProjectGrid: return "assets.browser";
                case EditorScrollPanelKind::Console: return "console";
                case EditorScrollPanelKind::Diagnostics: return "diagnostics";
                default: return "";
            }
        }

        bool IsPanelActiveInFrame(const EditorFrameLayout& frame, EditorPanelId panel)
        {
            for (const EditorDockNode& node : frame.dock.nodes)
            {
                if (node.kind != EditorDockNodeKind::Stack || node.tabs.empty())
                {
                    continue;
                }
                const std::size_t active = std::min(node.activeTab, node.tabs.size() - 1u);
                if (node.tabs[active].panel == panel)
                {
                    return true;
                }
            }
            return false;
        }

        bool IsPanelVisibleInFrame(const EditorFrameLayout& frame, EditorPanelId panel)
        {
            for (const EditorPanelPlacement& placement : frame.placements)
            {
                if (placement.panel == panel && placement.visible)
                {
                    return true;
                }
            }
            return false;
        }

        bool ParseEditorSession(std::string_view text, EditorSessionState& session)
        {
            std::istringstream input{std::string(text)};
            std::string magic;
            u32 version = 0;
            input >> magic >> version;
            if (magic != "AKEDITORSESSION" || version != 1)
            {
                return false;
            }

            session = {};
            session.schemaVersion = version;

            std::string tag;
            while (input >> tag)
            {
                if (tag == "project")
                {
                    input >> std::quoted(session.projectName);
                }
                else if (tag == "scene")
                {
                    input >> std::quoted(session.scenePath);
                }
                else if (tag == "focused")
                {
                    input >> std::quoted(session.focusedPanelName);
                }
                else if (tag == "panel")
                {
                    EditorPanelSessionState panel{};
                    input >> std::quoted(panel.panelName) >> panel.scrollX >> panel.scrollY >> panel.activeTab >> panel.visible >> panel.contentRevision;
                    session.panels.push_back(std::move(panel));
                }
                else if (tag == "select")
                {
                    int domain = 0;
                    EditorSessionSelectionEntry item{};
                    input >> domain >> item.active >> std::quoted(item.stableId) >> std::quoted(item.displayName);
                    item.domain = SelectionDomainFromInt(domain);
                    session.selection.push_back(std::move(item));
                }
                else if (tag == "expand")
                {
                    EditorSessionExpansionEntry item{};
                    input >> std::quoted(item.scope) >> item.expanded >> std::quoted(item.stableId);
                    session.expansions.push_back(std::move(item));
                }
                else if (tag == "foldout")
                {
                    EditorSessionFoldoutEntry item{};
                    input >> item.expanded >> std::quoted(item.key);
                    session.foldouts.push_back(std::move(item));
                }
                else if (tag == "hierarchy_search")
                {
                    input >> std::quoted(session.hierarchySearch);
                }
                else if (tag == "asset_search")
                {
                    input >> std::quoted(session.assetSearch);
                }
                else if (tag == "project_folder")
                {
                    input >> std::quoted(session.projectFolder);
                }
                else if (tag == "command_palette")
                {
                    input >> session.commandPaletteOpen >> std::quoted(session.commandPaletteQuery);
                }
                else if (tag == "transient")
                {
                    input >> session.popupOpen >> session.modalOpen >> session.textEditActive >> session.dragActive;
                }
                else if (tag == "revision")
                {
                    input >> session.revision;
                }
                else
                {
                    std::string rest;
                    std::getline(input, rest);
                }
            }

            return !session.projectName.empty();
        }
    }

    const char* ToString(EditorSessionRepairCode code)
    {
        switch (code)
        {
            case EditorSessionRepairCode::None: return "None";
            case EditorSessionRepairCode::UnsupportedVersion: return "UnsupportedVersion";
            case EditorSessionRepairCode::ParseFailed: return "ParseFailed";
            case EditorSessionRepairCode::UnknownPanel: return "UnknownPanel";
            case EditorSessionRepairCode::DuplicatePanel: return "DuplicatePanel";
            case EditorSessionRepairCode::InvalidFocusedPanel: return "InvalidFocusedPanel";
            case EditorSessionRepairCode::DuplicateSelection: return "DuplicateSelection";
            case EditorSessionRepairCode::UnknownSelection: return "UnknownSelection";
            case EditorSessionRepairCode::InvalidScrollOffset: return "InvalidScrollOffset";
            case EditorSessionRepairCode::InvalidExpansionKey: return "InvalidExpansionKey";
            case EditorSessionRepairCode::InvalidFoldoutKey: return "InvalidFoldoutKey";
            case EditorSessionRepairCode::TransientStateDropped: return "TransientStateDropped";
            case EditorSessionRepairCode::ResetToDefault: return "ResetToDefault";
            default: return "Unknown";
        }
    }

    const char* ToString(EditorSessionRepairSeverity severity)
    {
        switch (severity)
        {
            case EditorSessionRepairSeverity::Info: return "info";
            case EditorSessionRepairSeverity::Warning: return "warning";
            case EditorSessionRepairSeverity::Error: return "error";
            default: return "unknown";
        }
    }

    EditorSessionPersistencePolicy MakeDefaultEditorSessionPersistencePolicy()
    {
        return {};
    }

    EditorSessionState MakeDefaultEditorSessionState(const EditorPanelRegistry& panels, std::string projectName, std::string scenePath)
    {
        EditorSessionState session{};
        session.projectName = std::move(projectName);
        session.scenePath = std::move(scenePath);
        session.projectFolder = "Assets";
        session.revision = 1;

        const char* corePanels[] = {"scene.hierarchy", "scene.viewport", "inspector", "assets.browser", "console"};
        for (const char* name : corePanels)
        {
            const EditorPanelDesc* panel = panels.FindByName(name);
            if (!panel)
            {
                continue;
            }
            EditorPanelSessionState state{};
            state.panel = panel->id;
            state.panelName = panel->name;
            state.visible = true;
            state.activeTab = true;
            session.panels.push_back(std::move(state));
            if (!session.focusedPanel.IsValid())
            {
                session.focusedPanel = panel->id;
                session.focusedPanelName = panel->name;
            }
        }

        session.expansions.push_back({"hierarchy", "scene:Sandbox", true});
        session.foldouts.push_back({"inspector.transform", true});
        session.foldouts.push_back({"inspector.mesh", true});
        return session;
    }

    EditorSessionState CaptureEditorSessionState(const EditorSessionCaptureInput& input, const EditorPanelRegistry& panels)
    {
        EditorSessionState session{};
        session.projectName = input.projectName;
        session.scenePath = input.scenePath;
        session.hierarchySearch = input.hierarchySearch;
        session.assetSearch = input.assetSearch;
        session.projectFolder = input.projectFolder.empty() ? "Assets" : input.projectFolder;
        session.focusedPanel = input.focusedPanel;
        if (const EditorPanelDesc* focused = panels.Find(input.focusedPanel))
        {
            session.focusedPanelName = focused->name;
        }

        if (input.frame)
        {
            for (const EditorPanelDesc& panel : panels.Panels())
            {
                if (!IsPanelVisibleInFrame(*input.frame, panel.id))
                {
                    continue;
                }

                EditorPanelSessionState state{};
                state.panel = panel.id;
                state.panelName = panel.name;
                state.visible = true;
                state.activeTab = IsPanelActiveInFrame(*input.frame, panel.id);
                if (input.scrollStates)
                {
                    for (const EditorScrollState& scroll : *input.scrollStates)
                    {
                        if (panel.name == ScrollKindToPanelName(scroll.panel))
                        {
                            if (scroll.axis == EditorScrollAxis::Vertical)
                            {
                                state.scrollY = scroll.offsetPixels;
                            }
                            else
                            {
                                state.scrollX = scroll.offsetPixels;
                            }
                        }
                    }
                }
                session.panels.push_back(std::move(state));
            }
        }
        else
        {
            session = MakeDefaultEditorSessionState(panels, input.projectName, input.scenePath);
        }

        if (input.selection)
        {
            for (const EditorSelectionItem& item : input.selection->items)
            {
                EditorSessionSelectionEntry entry{};
                entry.domain = item.domain;
                entry.stableId = item.stableId;
                entry.displayName = item.displayName;
                entry.active = (item.stableId == input.selection->active.stableId && item.domain == input.selection->active.domain);
                session.selection.push_back(std::move(entry));
            }
        }

        session.expansions = input.expansions;
        session.foldouts = input.foldouts;
        session.revision = 1;
        return session;
    }

    std::string SerializeEditorSessionState(const EditorSessionState& session)
    {
        std::ostringstream out;
        out << "AKEDITORSESSION 1\n";
        out << "project " << std::quoted(session.projectName) << "\n";
        out << "scene " << std::quoted(session.scenePath) << "\n";
        out << "focused " << std::quoted(session.focusedPanelName) << "\n";
        for (const EditorPanelSessionState& panel : session.panels)
        {
            out << "panel " << std::quoted(panel.panelName) << ' ' << panel.scrollX << ' ' << panel.scrollY << ' ' << panel.activeTab << ' ' << panel.visible << ' ' << panel.contentRevision << "\n";
        }
        for (const EditorSessionSelectionEntry& item : session.selection)
        {
            out << "select " << static_cast<int>(item.domain) << ' ' << item.active << ' ' << std::quoted(item.stableId) << ' ' << std::quoted(item.displayName) << "\n";
        }
        for (const EditorSessionExpansionEntry& item : session.expansions)
        {
            out << "expand " << std::quoted(item.scope) << ' ' << item.expanded << ' ' << std::quoted(item.stableId) << "\n";
        }
        for (const EditorSessionFoldoutEntry& item : session.foldouts)
        {
            out << "foldout " << item.expanded << ' ' << std::quoted(item.key) << "\n";
        }
        out << "hierarchy_search " << std::quoted(session.hierarchySearch) << "\n";
        out << "asset_search " << std::quoted(session.assetSearch) << "\n";
        out << "project_folder " << std::quoted(session.projectFolder) << "\n";
        out << "command_palette " << session.commandPaletteOpen << ' ' << std::quoted(session.commandPaletteQuery) << "\n";
        out << "transient " << session.popupOpen << ' ' << session.modalOpen << ' ' << session.textEditActive << ' ' << session.dragActive << "\n";
        out << "revision " << session.revision << "\n";
        return out.str();
    }

    EditorSessionRepairReport RepairEditorSessionState(EditorSessionState& session, const EditorPanelRegistry& panels, const std::vector<std::string>& validStableIds, const EditorSessionPersistencePolicy& policy)
    {
        EditorSessionRepairReport report{};
        std::unordered_set<u32> seenPanels;
        std::vector<EditorPanelSessionState> repairedPanels;
        repairedPanels.reserve(session.panels.size());

        for (EditorPanelSessionState panel : session.panels)
        {
            const EditorPanelDesc* desc = panels.Find(panel.panel);
            if (!desc && !panel.panelName.empty())
            {
                desc = panels.FindByName(panel.panelName);
                if (desc)
                {
                    panel.panel = desc->id;
                    panel.panelName = desc->name;
                }
            }

            if (!desc)
            {
                AddIssue(report, EditorSessionRepairCode::UnknownPanel, EditorSessionRepairSeverity::Warning, "dropped unknown panel session state", panel.panel, panel.panelName);
                if (policy.dropUnknownPanels)
                {
                    continue;
                }
            }

            if (policy.dropDuplicatePanels && panel.panel.IsValid() && !seenPanels.insert(panel.panel.value).second)
            {
                AddIssue(report, EditorSessionRepairCode::DuplicatePanel, EditorSessionRepairSeverity::Warning, "dropped duplicate panel session state", panel.panel, panel.panelName);
                continue;
            }

            const i32 oldX = panel.scrollX;
            const i32 oldY = panel.scrollY;
            if (policy.clampScrollOffsets)
            {
                panel.scrollX = std::clamp(panel.scrollX, 0, policy.maxScrollPixels);
                panel.scrollY = std::clamp(panel.scrollY, 0, policy.maxScrollPixels);
            }
            if (oldX != panel.scrollX || oldY != panel.scrollY)
            {
                AddIssue(report, EditorSessionRepairCode::InvalidScrollOffset, EditorSessionRepairSeverity::Info, "clamped panel scroll offset", panel.panel, panel.panelName);
            }

            repairedPanels.push_back(std::move(panel));
            if (repairedPanels.size() >= policy.maxPanelStates)
            {
                break;
            }
        }

        if (policy.resetIfNoPanels && repairedPanels.empty())
        {
            session = MakeDefaultEditorSessionState(panels, session.projectName, session.scenePath);
            AddIssue(report, EditorSessionRepairCode::ResetToDefault, EditorSessionRepairSeverity::Error, "session had no valid panels and was reset");
            report.resetToDefault = true;
        }
        else
        {
            session.panels = std::move(repairedPanels);
        }

        const EditorPanelDesc* focused = panels.Find(session.focusedPanel);
        if (!focused && !session.focusedPanelName.empty())
        {
            focused = panels.FindByName(session.focusedPanelName);
            if (focused)
            {
                session.focusedPanel = focused->id;
                session.focusedPanelName = focused->name;
            }
        }
        if (!focused && !session.panels.empty())
        {
            session.focusedPanel = session.panels.front().panel;
            session.focusedPanelName = session.panels.front().panelName;
            AddIssue(report, EditorSessionRepairCode::InvalidFocusedPanel, EditorSessionRepairSeverity::Info, "focused panel was repaired", session.focusedPanel, session.focusedPanelName);
        }

        std::unordered_set<std::string> seenSelections;
        std::vector<EditorSessionSelectionEntry> repairedSelection;
        repairedSelection.reserve(session.selection.size());
        bool activeKept = false;
        for (EditorSessionSelectionEntry item : session.selection)
        {
            if (item.stableId.empty())
            {
                AddIssue(report, EditorSessionRepairCode::UnknownSelection, EditorSessionRepairSeverity::Warning, "dropped empty selection id");
                continue;
            }
            if (policy.dropUnknownSelections && !IsKnownStableId(validStableIds, item.stableId))
            {
                AddIssue(report, EditorSessionRepairCode::UnknownSelection, EditorSessionRepairSeverity::Warning, "dropped unknown selection target", {}, item.stableId);
                continue;
            }

            const std::string key = SelectionKey(item);
            if (policy.dropDuplicateSelections && !seenSelections.insert(key).second)
            {
                AddIssue(report, EditorSessionRepairCode::DuplicateSelection, EditorSessionRepairSeverity::Info, "dropped duplicate selection target", {}, item.stableId);
                continue;
            }

            if (item.active)
            {
                if (activeKept)
                {
                    item.active = false;
                }
                else
                {
                    activeKept = true;
                }
            }
            repairedSelection.push_back(std::move(item));
            if (repairedSelection.size() >= policy.maxSelectionEntries)
            {
                break;
            }
        }
        if (!activeKept && !repairedSelection.empty())
        {
            repairedSelection.front().active = true;
        }
        session.selection = std::move(repairedSelection);

        auto cleanExpansion = [&](EditorSessionExpansionEntry& item) {
            return !item.scope.empty() && !item.stableId.empty();
        };
        auto cleanFoldout = [&](EditorSessionFoldoutEntry& item) {
            return !item.key.empty();
        };

        std::vector<EditorSessionExpansionEntry> expansions;
        std::unordered_set<std::string> seenExpansions;
        for (EditorSessionExpansionEntry item : session.expansions)
        {
            if (!cleanExpansion(item))
            {
                AddIssue(report, EditorSessionRepairCode::InvalidExpansionKey, EditorSessionRepairSeverity::Info, "dropped invalid expansion key");
                continue;
            }
            const std::string key = item.scope + ":" + item.stableId;
            if (!seenExpansions.insert(key).second)
            {
                continue;
            }
            expansions.push_back(std::move(item));
            if (expansions.size() >= policy.maxExpansionEntries)
            {
                break;
            }
        }
        session.expansions = std::move(expansions);

        std::vector<EditorSessionFoldoutEntry> foldouts;
        std::unordered_set<std::string> seenFoldouts;
        for (EditorSessionFoldoutEntry item : session.foldouts)
        {
            if (!cleanFoldout(item))
            {
                AddIssue(report, EditorSessionRepairCode::InvalidFoldoutKey, EditorSessionRepairSeverity::Info, "dropped invalid foldout key");
                continue;
            }
            if (!seenFoldouts.insert(item.key).second)
            {
                continue;
            }
            foldouts.push_back(std::move(item));
            if (foldouts.size() >= policy.maxFoldoutEntries)
            {
                break;
            }
        }
        session.foldouts = std::move(foldouts);

        if (policy.dropTransientUiState && (session.commandPaletteOpen || session.popupOpen || session.modalOpen || session.textEditActive || session.dragActive))
        {
            session.commandPaletteOpen = false;
            session.popupOpen = false;
            session.modalOpen = false;
            session.textEditActive = false;
            session.dragActive = false;
            AddIssue(report, EditorSessionRepairCode::TransientStateDropped, EditorSessionRepairSeverity::Info, "dropped transient UI state from persisted session");
        }

        report.ok = !report.resetToDefault || !session.panels.empty();
        report.summary = FormatEditorSessionRepairReport(report);
        return report;
    }

    EditorSessionLoadResult LoadEditorSessionWithRepair(std::string_view text, const EditorPanelRegistry& panels, const std::vector<std::string>& validStableIds, const EditorSessionPersistencePolicy& policy)
    {
        EditorSessionLoadResult result{};
        if (!ParseEditorSession(text, result.session))
        {
            result.session = MakeDefaultEditorSessionState(panels);
            AddIssue(result.repair, EditorSessionRepairCode::ParseFailed, EditorSessionRepairSeverity::Error, "session parse failed; reset to default");
            result.repair.resetToDefault = true;
            result.repair.ok = true;
            result.repair.summary = FormatEditorSessionRepairReport(result.repair);
            result.loadedFromText = false;
            result.ok = true;
            result.summary = "editor-session load=parse-failed reset=1 ok=1";
            return result;
        }

        result.loadedFromText = true;
        result.repair = RepairEditorSessionState(result.session, panels, validStableIds, policy);
        result.ok = result.repair.ok;
        std::ostringstream out;
        out << "editor-session load=1 panels=" << result.session.panels.size()
            << " selection=" << result.session.selection.size()
            << " repaired=" << (result.repair.repaired ? 1 : 0)
            << " issues=" << result.repair.issues.size()
            << " ok=" << (result.ok ? 1 : 0);
        result.summary = out.str();
        return result;
    }

    bool RestoreEditorSessionActiveTabs(EditorFrameLayout& frame, const EditorSessionState& session, const EditorPanelRegistry& panels)
    {
        bool restored = false;
        for (const EditorPanelSessionState& panelState : session.panels)
        {
            if (!panelState.activeTab)
            {
                continue;
            }
            const EditorPanelDesc* panel = panels.Find(panelState.panel);
            if (!panel && !panelState.panelName.empty())
            {
                panel = panels.FindByName(panelState.panelName);
            }
            if (panel && ActivateDockTab(frame.dock, panel->id))
            {
                restored = true;
            }
        }
        if (restored)
        {
            ComputeEditorDockRects(frame);
        }
        return restored;
    }

    std::vector<EditorScrollState> RestoreEditorSessionScrollStates(const EditorSessionState& session)
    {
        std::vector<EditorScrollState> states;
        for (const EditorPanelSessionState& panel : session.panels)
        {
            const EditorScrollPanelKind kind = PanelNameToScrollKind(panel.panelName);
            if (kind == EditorScrollPanelKind::Unknown)
            {
                continue;
            }
            EditorScrollState state = MakeEditorScrollState(kind, panel.scrollY + 800, 400, 22);
            state.offsetPixels = panel.scrollY;
            state.overflow = state.contentPixels > state.viewportPixels;
            states.push_back(state);
        }
        return states;
    }

    EditorSelectionModel RestoreEditorSessionSelection(const EditorSessionState& session)
    {
        EditorSelectionModel model{};
        for (const EditorSessionSelectionEntry& entry : session.selection)
        {
            EditorSelectionItem item{};
            item.domain = entry.domain;
            item.stableId = entry.stableId;
            item.displayName = entry.displayName;
            model.items.push_back(item);
            if (entry.active)
            {
                model.active = item;
            }
        }
        if (model.active.stableId.empty() && !model.items.empty())
        {
            model.active = model.items.front();
        }
        model.revision++;
        return model;
    }

    std::string FormatEditorSessionRepairIssue(const EditorSessionRepairIssue& issue)
    {
        std::ostringstream out;
        out << ToString(issue.severity) << ':' << ToString(issue.code);
        if (issue.panel.IsValid())
        {
            out << " panel=" << issue.panel.value;
        }
        if (!issue.stableId.empty())
        {
            out << " id=" << issue.stableId;
        }
        if (!issue.message.empty())
        {
            out << " " << issue.message;
        }
        return out.str();
    }

    std::string FormatEditorSessionRepairReport(const EditorSessionRepairReport& report)
    {
        std::ostringstream out;
        out << "editor-session-repair issues=" << report.issues.size()
            << " repaired=" << (report.repaired ? 1 : 0)
            << " reset=" << (report.resetToDefault ? 1 : 0)
            << " ok=" << (report.ok ? 1 : 0);
        return out.str();
    }

    EditorSessionDiagnostics RunEditorSessionDiagnostics()
    {
        EditorSessionDiagnostics diagnostics{};
        const EditorPanelRegistry panels = BuildDefaultEditorPanelRegistry();
        EditorFrameLayout frame = BuildDefaultEditorFrameLayout(panels, 1600, 900);
        ComputeEditorDockRects(frame);

        EditorSelectionModel selection{};
        EditorSelectionItem camera{};
        camera.domain = EditorSelectionDomain::SceneEntity;
        camera.stableId = "entity:camera";
        camera.displayName = "Main Camera";
        SetSelection(selection, camera);

        std::vector<EditorScrollState> scrolls;
        scrolls.push_back(MakeEditorScrollState(EditorScrollPanelKind::Hierarchy, 1400, 300));
        scrolls.back().offsetPixels = 88;
        scrolls.push_back(MakeEditorScrollState(EditorScrollPanelKind::Inspector, 1800, 420));
        scrolls.back().offsetPixels = 132;
        scrolls.push_back(MakeEditorScrollState(EditorScrollPanelKind::ProjectGrid, 1200, 240));
        scrolls.back().offsetPixels = 176;

        EditorSessionCaptureInput capture{};
        capture.frame = &frame;
        capture.selection = &selection;
        capture.scrollStates = &scrolls;
        capture.focusedPanel = panels.FindByName("scene.viewport") ? panels.FindByName("scene.viewport")->id : EditorPanelId{};
        capture.expansions = {{"hierarchy", "scene:Sandbox", true}, {"hierarchy", "entity:camera", true}};
        capture.foldouts = {{"inspector.transform", true}, {"inspector.camera", true}};
        EditorSessionState session = CaptureEditorSessionState(capture, panels);
        session.popupOpen = true;
        session.textEditActive = true;
        session.commandPaletteOpen = true;
        session.commandPaletteQuery = "create light";

        const std::string serialized = SerializeEditorSessionState(session);
        const std::vector<std::string> validIds = {"entity:camera", "scene:Sandbox"};
        const EditorSessionLoadResult load = LoadEditorSessionWithRepair(serialized, panels, validIds);

        EditorFrameLayout restoredFrame = frame;
        diagnostics.activeTabsRestored = RestoreEditorSessionActiveTabs(restoredFrame, load.session, panels);
        const std::vector<EditorScrollState> restoredScrolls = RestoreEditorSessionScrollStates(load.session);
        const EditorSelectionModel restoredSelection = RestoreEditorSessionSelection(load.session);

        diagnostics.panelStateCount = load.session.panels.size();
        diagnostics.selectionCount = restoredSelection.items.size();
        diagnostics.expansionCount = load.session.expansions.size();
        diagnostics.foldoutCount = load.session.foldouts.size();
        diagnostics.issueCount = load.repair.issues.size();
        diagnostics.serialized = !serialized.empty();
        diagnostics.loaded = load.ok;
        diagnostics.repaired = load.repair.repaired;
        diagnostics.transientDropped = !load.session.popupOpen && !load.session.textEditActive && !load.session.commandPaletteOpen;
        diagnostics.ok = diagnostics.serialized
            && diagnostics.loaded
            && diagnostics.panelStateCount >= 5
            && diagnostics.selectionCount == 1
            && !restoredScrolls.empty()
            && diagnostics.transientDropped
            && diagnostics.activeTabsRestored;
        diagnostics.summary = FormatEditorSessionDiagnostics(diagnostics);
        return diagnostics;
    }

    std::string FormatEditorSessionDiagnostics(const EditorSessionDiagnostics& diagnostics)
    {
        std::ostringstream out;
        out << "editor-session panels=" << diagnostics.panelStateCount
            << " selection=" << diagnostics.selectionCount
            << " expansions=" << diagnostics.expansionCount
            << " foldouts=" << diagnostics.foldoutCount
            << " issues=" << diagnostics.issueCount
            << " serialized=" << (diagnostics.serialized ? 1 : 0)
            << " loaded=" << (diagnostics.loaded ? 1 : 0)
            << " transientDropped=" << (diagnostics.transientDropped ? 1 : 0)
            << " activeTabs=" << (diagnostics.activeTabsRestored ? 1 : 0)
            << " ok=" << (diagnostics.ok ? 1 : 0);
        return out.str();
    }
}
