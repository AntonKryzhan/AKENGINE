#pragma once

#include <AK/Core/Types.hpp>

#include <string>
#include <vector>

namespace AK
{
    struct EditorPanelId final
    {
        u32 value = 0;

        constexpr bool IsValid() const
        {
            return value != 0;
        }
    };

    constexpr bool operator==(EditorPanelId a, EditorPanelId b)
    {
        return a.value == b.value;
    }

    constexpr bool operator!=(EditorPanelId a, EditorPanelId b)
    {
        return !(a == b);
    }

    enum class EditorPanelKind
    {
        SceneHierarchy,
        SceneViewport,
        GameViewport,
        Inspector,
        AssetBrowser,
        Console,
        Diagnostics,
        RenderGraphDebugger,
        WorldPartition,
        TerrainEditor,
        MaterialEditor,
        PhysicsDebugger,
        Settings,
        CommandPalette,
        Custom
    };

    enum class EditorPanelCapability : u32
    {
        None = 0,
        Dockable = 1u << 0u,
        Closable = 1u << 1u,
        Singleton = 1u << 2u,
        Focusable = 1u << 3u,
        Searchable = 1u << 4u,
        RuntimeOnly = 1u << 5u,
        RequiresScene = 1u << 6u,
        RequiresSelection = 1u << 7u,
        HeavyDiagnostics = 1u << 8u
    };

    constexpr EditorPanelCapability operator|(EditorPanelCapability a, EditorPanelCapability b)
    {
        return static_cast<EditorPanelCapability>(static_cast<u32>(a) | static_cast<u32>(b));
    }

    constexpr EditorPanelCapability operator&(EditorPanelCapability a, EditorPanelCapability b)
    {
        return static_cast<EditorPanelCapability>(static_cast<u32>(a) & static_cast<u32>(b));
    }

    struct EditorPanelDesc
    {
        EditorPanelId id{};
        EditorPanelKind kind = EditorPanelKind::Custom;
        std::string name;
        std::string title;
        std::string category;
        std::string icon;
        EditorPanelCapability capabilities = EditorPanelCapability::None;
        bool defaultVisible = true;
        u32 defaultOrder = 0;
    };

    struct EditorPanelDiagnostics
    {
        std::size_t panelCount = 0;
        std::size_t singletonCount = 0;
        std::size_t dockableCount = 0;
        std::size_t searchableCount = 0;
        std::size_t duplicateIdCount = 0;
        std::size_t duplicateNameCount = 0;
        std::size_t missingCorePanelCount = 0;
        bool ok = false;
        std::string summary;
    };

    class EditorPanelRegistry final
    {
    public:
        EditorPanelId Add(EditorPanelDesc desc);
        const EditorPanelDesc* Find(EditorPanelId id) const;
        const EditorPanelDesc* FindByName(const std::string& name) const;
        bool Has(EditorPanelId id) const;
        const std::vector<EditorPanelDesc>& Panels() const;

    private:
        std::vector<EditorPanelDesc> mPanels;
        u32 mNextPanelId = 1;
    };

    const char* ToString(EditorPanelKind kind);
    bool HasCapability(EditorPanelCapability value, EditorPanelCapability flag);
    EditorPanelRegistry BuildDefaultEditorPanelRegistry();
    EditorPanelDiagnostics BuildEditorPanelDiagnostics(const EditorPanelRegistry& registry);
    std::string FormatEditorPanel(const EditorPanelDesc& panel);
}
