#pragma once

#include <AK/Core/Types.hpp>
#include <AK/ECS/EntityId.hpp>
#include <AK/EditorUI/EditorRuntimeBridge.hpp>

#include <string>
#include <vector>

namespace AK
{
    enum class EditorPropertyType
    {
        Bool,
        Int,
        Float,
        Double,
        String,
        Vec3,
        Color,
        AssetGuid,
        EntityRef,
        Enum,
        Section
    };

    enum class EditorPropertyFlag : u32
    {
        None = 0,
        ReadOnly = 1u << 0u,
        Advanced = 1u << 1u,
        Dirty = 1u << 2u,
        Animatable = 1u << 3u,
        Serialized = 1u << 4u,
        RequiresRebuild = 1u << 5u
    };

    constexpr EditorPropertyFlag operator|(EditorPropertyFlag a, EditorPropertyFlag b)
    {
        return static_cast<EditorPropertyFlag>(static_cast<u32>(a) | static_cast<u32>(b));
    }

    constexpr EditorPropertyFlag operator&(EditorPropertyFlag a, EditorPropertyFlag b)
    {
        return static_cast<EditorPropertyFlag>(static_cast<u32>(a) & static_cast<u32>(b));
    }

    struct EditorPropertyValue
    {
        EditorPropertyType type = EditorPropertyType::String;
        std::string text;
        double numbers[4] = {0.0, 0.0, 0.0, 0.0};
        bool boolean = false;
    };

    struct EditorPropertyDesc
    {
        std::string path;
        std::string label;
        std::string tooltip;
        EditorPropertyValue value{};
        EditorPropertyFlag flags = EditorPropertyFlag::Serialized;
        float minValue = 0.0f;
        float maxValue = 0.0f;
        float stepValue = 0.0f;
        bool hasRange = false;
        bool hasStep = false;
    };

    struct EditorComponentInspector
    {
        std::string componentType;
        std::string displayName;
        bool expanded = true;
        bool removable = true;
        std::vector<EditorPropertyDesc> properties;
    };

    struct EditorInspectorModel
    {
        EditorSelectionSnapshot selection{};
        std::vector<EditorComponentInspector> components;
        std::string header;
        bool multiEdit = false;
        bool locked = false;
        u64 revision = 1;
    };

    struct EditorHierarchyNode
    {
        EntityId entity{};
        std::string stableId;
        std::string name;
        std::string icon;
        i32 depth = 0;
        bool expanded = true;
        bool selected = false;
        bool active = true;
        bool prefabInstance = false;
        std::vector<EditorHierarchyNode> children;
    };

    struct EditorHierarchyModel
    {
        std::vector<EditorHierarchyNode> roots;
        std::string filter;
        std::size_t visibleNodeCount = 0;
        u64 revision = 1;
    };

    enum class EditorAssetKind
    {
        Folder,
        Scene,
        Mesh,
        Material,
        Texture,
        Shader,
        Script,
        Audio,
        PointCloud,
        Unknown
    };

    struct EditorAssetItem
    {
        std::string guid;
        std::string name;
        std::string logicalPath;
        EditorAssetKind kind = EditorAssetKind::Unknown;
        u64 sizeBytes = 0;
        u64 contentHash = 0;
        bool folder = false;
        bool dirty = false;
        bool missing = false;
    };

    struct EditorAssetBrowserModel
    {
        std::string rootPath;
        std::string currentPath;
        std::string searchText;
        std::vector<std::string> breadcrumbs;
        std::vector<EditorAssetItem> items;
        std::size_t selectedIndex = 0;
        u64 revision = 1;
    };

    enum class EditorConsoleSeverity
    {
        Info,
        Warning,
        Error
    };

    struct EditorConsoleEntry
    {
        EditorConsoleSeverity severity = EditorConsoleSeverity::Info;
        std::string channel;
        std::string message;
        u64 frame = 0;
        u64 repeatCount = 1;
    };

    struct EditorConsoleModel
    {
        std::vector<EditorConsoleEntry> entries;
        bool showInfo = true;
        bool showWarnings = true;
        bool showErrors = true;
        std::string filter;
        u64 revision = 1;
    };

    enum class EditorMetricUnit
    {
        Count,
        Bytes,
        Milliseconds,
        Percent
    };

    struct EditorMetric
    {
        std::string key;
        std::string label;
        double value = 0.0;
        EditorMetricUnit unit = EditorMetricUnit::Count;
        bool warning = false;
    };

    struct EditorDiagnosticsPanelModel
    {
        std::vector<EditorMetric> metrics;
        std::vector<std::string> recentEvents;
        u64 frameIndex = 0;
        u64 revision = 1;
    };

    struct EditorPanelModelFrame
    {
        EditorInspectorModel inspector{};
        EditorHierarchyModel hierarchy{};
        EditorAssetBrowserModel assets{};
        EditorConsoleModel console{};
        EditorDiagnosticsPanelModel diagnostics{};
        EditorSearchIndex searchIndex{};
        u64 revision = 1;
    };

    struct EditorPanelModelDiagnostics
    {
        std::size_t hierarchyNodeCount = 0;
        std::size_t componentCount = 0;
        std::size_t propertyCount = 0;
        std::size_t dirtyPropertyCount = 0;
        std::size_t rangedPropertyCount = 0;
        std::size_t steppedPropertyCount = 0;
        std::size_t readOnlyPropertyCount = 0;
        std::size_t assetCount = 0;
        std::size_t missingAssetCount = 0;
        std::size_t consoleErrorCount = 0;
        std::size_t metricCount = 0;
        std::size_t searchRecordCount = 0;
        bool ok = false;
        std::string summary;
    };

    const char* ToString(EditorPropertyType type);
    const char* ToString(EditorAssetKind kind);
    const char* ToString(EditorConsoleSeverity severity);
    const char* ToString(EditorMetricUnit unit);
    bool HasFlag(EditorPropertyFlag value, EditorPropertyFlag flag);

    EditorPropertyValue MakeBoolPropertyValue(bool value);
    EditorPropertyValue MakeNumberPropertyValue(EditorPropertyType type, double x, double y = 0.0, double z = 0.0, double w = 0.0);
    EditorPropertyValue MakeStringPropertyValue(EditorPropertyType type, std::string text);

    EditorPanelModelFrame BuildDefaultEditorPanelModelFrame(const EditorRuntimeBridge& bridge);
    EditorPanelModelDiagnostics ValidateEditorPanelModelFrame(const EditorPanelModelFrame& frame);
    EditorSearchIndex BuildPanelModelSearchIndex(const EditorPanelModelFrame& frame, const EditorSearchIndex& baseIndex);

    std::string FormatEditorProperty(const EditorPropertyDesc& property);
    std::string FormatEditorHierarchyNode(const EditorHierarchyNode& node);
    std::string FormatEditorAssetItem(const EditorAssetItem& item);
    std::string FormatEditorConsoleEntry(const EditorConsoleEntry& entry);
    std::string FormatEditorMetric(const EditorMetric& metric);
    std::string FormatEditorPanelModelDiagnostics(const EditorPanelModelDiagnostics& diagnostics);
}
