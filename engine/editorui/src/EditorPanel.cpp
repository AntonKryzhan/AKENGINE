#include <AK/EditorUI/EditorPanel.hpp>

#include <algorithm>
#include <sstream>
#include <unordered_set>

namespace AK
{
    namespace
    {
        u32 PanelIndex(EditorPanelKind kind)
        {
            return static_cast<u32>(kind);
        }

        void AddDefaultPanel(EditorPanelRegistry& registry, EditorPanelKind kind, const char* name, const char* title, const char* category, const char* icon, EditorPanelCapability capabilities, bool visible, u32 order)
        {
            EditorPanelDesc desc{};
            desc.kind = kind;
            desc.name = name;
            desc.title = title;
            desc.category = category;
            desc.icon = icon;
            desc.capabilities = capabilities;
            desc.defaultVisible = visible;
            desc.defaultOrder = order;
            registry.Add(std::move(desc));
        }
    }

    EditorPanelId EditorPanelRegistry::Add(EditorPanelDesc desc)
    {
        if (!desc.id.IsValid())
        {
            desc.id.value = mNextPanelId++;
        }
        else
        {
            mNextPanelId = std::max(mNextPanelId, desc.id.value + 1u);
        }

        mPanels.push_back(std::move(desc));
        return mPanels.back().id;
    }

    const EditorPanelDesc* EditorPanelRegistry::Find(EditorPanelId id) const
    {
        for (const EditorPanelDesc& panel : mPanels)
        {
            if (panel.id == id)
            {
                return &panel;
            }
        }
        return nullptr;
    }

    const EditorPanelDesc* EditorPanelRegistry::FindByName(const std::string& name) const
    {
        for (const EditorPanelDesc& panel : mPanels)
        {
            if (panel.name == name)
            {
                return &panel;
            }
        }
        return nullptr;
    }

    bool EditorPanelRegistry::Has(EditorPanelId id) const
    {
        return Find(id) != nullptr;
    }

    const std::vector<EditorPanelDesc>& EditorPanelRegistry::Panels() const
    {
        return mPanels;
    }

    const char* ToString(EditorPanelKind kind)
    {
        switch (kind)
        {
            case EditorPanelKind::SceneHierarchy: return "SceneHierarchy";
            case EditorPanelKind::SceneViewport: return "SceneViewport";
            case EditorPanelKind::GameViewport: return "GameViewport";
            case EditorPanelKind::Inspector: return "Inspector";
            case EditorPanelKind::AssetBrowser: return "AssetBrowser";
            case EditorPanelKind::Console: return "Console";
            case EditorPanelKind::Diagnostics: return "Diagnostics";
            case EditorPanelKind::RenderGraphDebugger: return "RenderGraphDebugger";
            case EditorPanelKind::WorldPartition: return "WorldPartition";
            case EditorPanelKind::TerrainEditor: return "TerrainEditor";
            case EditorPanelKind::MaterialEditor: return "MaterialEditor";
            case EditorPanelKind::PhysicsDebugger: return "PhysicsDebugger";
            case EditorPanelKind::Settings: return "Settings";
            case EditorPanelKind::CommandPalette: return "CommandPalette";
            case EditorPanelKind::Custom: return "Custom";
            default: return "Unknown";
        }
    }

    bool HasCapability(EditorPanelCapability value, EditorPanelCapability flag)
    {
        return (static_cast<u32>(value & flag) != 0u);
    }

    EditorPanelRegistry BuildDefaultEditorPanelRegistry()
    {
        EditorPanelRegistry registry;
        const EditorPanelCapability base = EditorPanelCapability::Dockable | EditorPanelCapability::Singleton | EditorPanelCapability::Focusable | EditorPanelCapability::Searchable;
        const EditorPanelCapability closable = base | EditorPanelCapability::Closable;
        const EditorPanelCapability scene = base | EditorPanelCapability::RequiresScene;
        const EditorPanelCapability selection = base | EditorPanelCapability::RequiresScene | EditorPanelCapability::RequiresSelection;
        const EditorPanelCapability diagnostics = closable | EditorPanelCapability::HeavyDiagnostics;

        AddDefaultPanel(registry, EditorPanelKind::SceneHierarchy, "scene.hierarchy", "Hierarchy", "Scene", "tree", scene, true, 10);
        AddDefaultPanel(registry, EditorPanelKind::SceneViewport, "scene.viewport", "Scene", "Viewport", "scene", scene, true, 20);
        AddDefaultPanel(registry, EditorPanelKind::GameViewport, "game.viewport", "Game", "Viewport", "game", scene, true, 21);
        AddDefaultPanel(registry, EditorPanelKind::Inspector, "inspector", "Inspector", "Scene", "sliders", selection, true, 30);
        AddDefaultPanel(registry, EditorPanelKind::AssetBrowser, "assets.browser", "Project", "Assets", "folder", base, true, 40);
        AddDefaultPanel(registry, EditorPanelKind::Console, "console", "Console", "Diagnostics", "terminal", base, true, 50);
        AddDefaultPanel(registry, EditorPanelKind::Diagnostics, "diagnostics", "Diagnostics", "Diagnostics", "pulse", diagnostics, true, 60);
        AddDefaultPanel(registry, EditorPanelKind::RenderGraphDebugger, "render.graph", "RenderGraph", "Rendering", "graph", diagnostics, true, 70);
        AddDefaultPanel(registry, EditorPanelKind::WorldPartition, "world.partition", "World Partition", "World", "grid", closable | EditorPanelCapability::RequiresScene, true, 80);
        AddDefaultPanel(registry, EditorPanelKind::TerrainEditor, "terrain.editor", "Terrain", "World", "mountain", closable | EditorPanelCapability::RequiresScene, false, 90);
        AddDefaultPanel(registry, EditorPanelKind::MaterialEditor, "material.editor", "Material", "Assets", "material", closable, false, 100);
        AddDefaultPanel(registry, EditorPanelKind::PhysicsDebugger, "physics.debugger", "Physics", "Diagnostics", "physics", diagnostics | EditorPanelCapability::RequiresScene, false, 110);
        AddDefaultPanel(registry, EditorPanelKind::Settings, "project.settings", "Project Settings", "Project", "gear", closable, false, 120);
        AddDefaultPanel(registry, EditorPanelKind::CommandPalette, "command.palette", "Command Palette", "Editor", "search", EditorPanelCapability::Focusable | EditorPanelCapability::Searchable | EditorPanelCapability::Singleton, false, 130);
        return registry;
    }

    EditorPanelDiagnostics BuildEditorPanelDiagnostics(const EditorPanelRegistry& registry)
    {
        EditorPanelDiagnostics diagnostics{};
        diagnostics.panelCount = registry.Panels().size();

        std::unordered_set<u32> ids;
        std::unordered_set<std::string> names;
        std::unordered_set<u32> coreKinds;
        for (const EditorPanelDesc& panel : registry.Panels())
        {
            if (!panel.id.IsValid() || !ids.insert(panel.id.value).second)
            {
                ++diagnostics.duplicateIdCount;
            }
            if (panel.name.empty() || !names.insert(panel.name).second)
            {
                ++diagnostics.duplicateNameCount;
            }
            if (HasCapability(panel.capabilities, EditorPanelCapability::Singleton))
            {
                ++diagnostics.singletonCount;
            }
            if (HasCapability(panel.capabilities, EditorPanelCapability::Dockable))
            {
                ++diagnostics.dockableCount;
            }
            if (HasCapability(panel.capabilities, EditorPanelCapability::Searchable))
            {
                ++diagnostics.searchableCount;
            }
            coreKinds.insert(PanelIndex(panel.kind));
        }

        const EditorPanelKind required[] = {
            EditorPanelKind::SceneHierarchy,
            EditorPanelKind::SceneViewport,
            EditorPanelKind::Inspector,
            EditorPanelKind::AssetBrowser,
            EditorPanelKind::Console
        };
        for (EditorPanelKind kind : required)
        {
            if (coreKinds.count(PanelIndex(kind)) == 0)
            {
                ++diagnostics.missingCorePanelCount;
            }
        }

        diagnostics.ok = diagnostics.panelCount >= 5
            && diagnostics.duplicateIdCount == 0
            && diagnostics.duplicateNameCount == 0
            && diagnostics.missingCorePanelCount == 0;

        std::ostringstream out;
        out << "panels=" << diagnostics.panelCount
            << " dockable=" << diagnostics.dockableCount
            << " searchable=" << diagnostics.searchableCount
            << " singleton=" << diagnostics.singletonCount
            << " duplicates=" << (diagnostics.duplicateIdCount + diagnostics.duplicateNameCount)
            << " missingCore=" << diagnostics.missingCorePanelCount
            << " ok=" << (diagnostics.ok ? "true" : "false");
        diagnostics.summary = out.str();
        return diagnostics;
    }

    std::string FormatEditorPanel(const EditorPanelDesc& panel)
    {
        std::ostringstream out;
        out << panel.id.value << ':' << panel.name << " title=" << panel.title
            << " kind=" << ToString(panel.kind)
            << " category=" << panel.category
            << " visible=" << (panel.defaultVisible ? "true" : "false");
        return out.str();
    }
}
