#include <AK/EditorUI/EditorPanelModels.hpp>

#include <algorithm>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace AK
{
    namespace
    {
        u64 HashText(std::string_view text)
        {
            u64 hash = 1469598103934665603ull;
            for (char c : text)
            {
                hash ^= static_cast<unsigned char>(c);
                hash *= 1099511628211ull;
            }
            return hash;
        }

        void CountHierarchyNode(const EditorHierarchyNode& node, std::size_t& count)
        {
            ++count;
            for (const EditorHierarchyNode& child : node.children)
            {
                CountHierarchyNode(child, count);
            }
        }

        void AddSearchRecord(EditorSearchIndex& index, EditorSearchResultKind kind, std::string title, std::string subtitle, std::string stableId, float score)
        {
            EditorSearchResult result{};
            result.kind = kind;
            result.title = std::move(title);
            result.subtitle = std::move(subtitle);
            result.stableId = std::move(stableId);
            result.score = score;
            index.records.push_back(std::move(result));
        }

        EditorPropertyDesc MakeProperty(const char* path, const char* label, EditorPropertyValue value, EditorPropertyFlag flags = EditorPropertyFlag::Serialized, const char* tooltip = "")
        {
            EditorPropertyDesc property{};
            property.path = path;
            property.label = label;
            property.tooltip = tooltip;
            property.value = std::move(value);
            property.flags = flags;
            return property;
        }

        EditorPropertyDesc MakeRangedProperty(const char* path, const char* label, EditorPropertyValue value, float minValue, float maxValue, float stepValue, EditorPropertyFlag flags = EditorPropertyFlag::Serialized, const char* tooltip = "")
        {
            EditorPropertyDesc property = MakeProperty(path, label, std::move(value), flags, tooltip);
            property.minValue = minValue;
            property.maxValue = maxValue;
            property.stepValue = stepValue;
            property.hasRange = true;
            property.hasStep = stepValue > 0.0f;
            return property;
        }

        EditorHierarchyNode MakeEntityNode(u32 index, u32 generation, const char* name, i32 depth, bool selected)
        {
            EditorHierarchyNode node{};
            node.entity = EntityId{index, generation};
            node.stableId = std::string("entity:") + std::to_string(index) + ':' + std::to_string(generation);
            node.name = name;
            node.icon = "entity";
            node.depth = depth;
            node.selected = selected;
            return node;
        }
    }

    const char* ToString(EditorPropertyType type)
    {
        switch (type)
        {
            case EditorPropertyType::Bool: return "Bool";
            case EditorPropertyType::Int: return "Int";
            case EditorPropertyType::Float: return "Float";
            case EditorPropertyType::Double: return "Double";
            case EditorPropertyType::String: return "String";
            case EditorPropertyType::Vec3: return "Vec3";
            case EditorPropertyType::Color: return "Color";
            case EditorPropertyType::AssetGuid: return "AssetGuid";
            case EditorPropertyType::EntityRef: return "EntityRef";
            case EditorPropertyType::Enum: return "Enum";
            case EditorPropertyType::Section: return "Section";
            default: return "Unknown";
        }
    }

    const char* ToString(EditorAssetKind kind)
    {
        switch (kind)
        {
            case EditorAssetKind::Folder: return "Folder";
            case EditorAssetKind::Scene: return "Scene";
            case EditorAssetKind::Mesh: return "Mesh";
            case EditorAssetKind::Material: return "Material";
            case EditorAssetKind::Texture: return "Texture";
            case EditorAssetKind::Shader: return "Shader";
            case EditorAssetKind::Script: return "Script";
            case EditorAssetKind::Audio: return "Audio";
            case EditorAssetKind::PointCloud: return "PointCloud";
            case EditorAssetKind::Unknown: return "Unknown";
            default: return "Unknown";
        }
    }

    const char* ToString(EditorConsoleSeverity severity)
    {
        switch (severity)
        {
            case EditorConsoleSeverity::Info: return "Info";
            case EditorConsoleSeverity::Warning: return "Warning";
            case EditorConsoleSeverity::Error: return "Error";
            default: return "Unknown";
        }
    }

    const char* ToString(EditorMetricUnit unit)
    {
        switch (unit)
        {
            case EditorMetricUnit::Count: return "count";
            case EditorMetricUnit::Bytes: return "bytes";
            case EditorMetricUnit::Milliseconds: return "ms";
            case EditorMetricUnit::Percent: return "%";
            default: return "unknown";
        }
    }

    bool HasFlag(EditorPropertyFlag value, EditorPropertyFlag flag)
    {
        return (static_cast<u32>(value & flag) != 0u);
    }

    EditorPropertyValue MakeBoolPropertyValue(bool value)
    {
        EditorPropertyValue out{};
        out.type = EditorPropertyType::Bool;
        out.boolean = value;
        out.text = value ? "true" : "false";
        return out;
    }

    EditorPropertyValue MakeNumberPropertyValue(EditorPropertyType type, double x, double y, double z, double w)
    {
        EditorPropertyValue out{};
        out.type = type;
        out.numbers[0] = x;
        out.numbers[1] = y;
        out.numbers[2] = z;
        out.numbers[3] = w;
        std::ostringstream text;
        if (type == EditorPropertyType::Vec3)
        {
            text << x << ',' << y << ',' << z;
        }
        else if (type == EditorPropertyType::Color)
        {
            text << x << ',' << y << ',' << z << ',' << w;
        }
        else
        {
            text << x;
        }
        out.text = text.str();
        return out;
    }

    EditorPropertyValue MakeStringPropertyValue(EditorPropertyType type, std::string text)
    {
        EditorPropertyValue out{};
        out.type = type;
        out.text = std::move(text);
        return out;
    }

    EditorPanelModelFrame BuildDefaultEditorPanelModelFrame(const EditorRuntimeBridge& bridge)
    {
        EditorPanelModelFrame frame{};
        frame.revision = bridge.revision;
        frame.inspector.selection = CaptureSelection(bridge.selection);
        frame.inspector.header = "Entity: CameraRig";
        frame.inspector.multiEdit = bridge.selection.items.size() > 1;

        EditorComponentInspector transform{};
        transform.componentType = "TransformComponent";
        transform.displayName = "Transform";
        transform.removable = false;
        transform.properties.push_back(MakeRangedProperty("Transform.Position", "Position", MakeNumberPropertyValue(EditorPropertyType::Vec3, 0.0, 1.8, -6.0), -100000.0f, 100000.0f, 0.1f, EditorPropertyFlag::Serialized | EditorPropertyFlag::Animatable | EditorPropertyFlag::Dirty));
        transform.properties.push_back(MakeRangedProperty("Transform.Rotation", "Rotation", MakeNumberPropertyValue(EditorPropertyType::Vec3, 20.0, 0.0, 0.0), -360.0f, 360.0f, 0.1f, EditorPropertyFlag::Serialized | EditorPropertyFlag::Animatable));
        transform.properties.push_back(MakeRangedProperty("Transform.Scale", "Scale", MakeNumberPropertyValue(EditorPropertyType::Vec3, 1.0, 1.0, 1.0), 0.0001f, 1000.0f, 0.01f, EditorPropertyFlag::Serialized | EditorPropertyFlag::Animatable));
        transform.properties.push_back(MakeProperty("Transform.WorldRevision", "World Revision", MakeNumberPropertyValue(EditorPropertyType::Int, 42.0), EditorPropertyFlag::ReadOnly));
        frame.inspector.components.push_back(std::move(transform));

        EditorComponentInspector worldPosition{};
        worldPosition.componentType = "WorldPositionComponent";
        worldPosition.displayName = "World Position";
        worldPosition.properties.push_back(MakeProperty("WorldPosition.Cell", "Cell", MakeNumberPropertyValue(EditorPropertyType::Vec3, 0.0, 0.0, 0.0), EditorPropertyFlag::ReadOnly));
        worldPosition.properties.push_back(MakeRangedProperty("WorldPosition.Local", "Local Offset", MakeNumberPropertyValue(EditorPropertyType::Vec3, 0.0, 1.8, -6.0), -100000.0f, 100000.0f, 0.1f));
        worldPosition.properties.push_back(MakeProperty("WorldPosition.Authoritative", "Authoritative", MakeBoolPropertyValue(true)));
        frame.inspector.components.push_back(std::move(worldPosition));

        EditorComponentInspector camera{};
        camera.componentType = "CameraComponent";
        camera.displayName = "Camera";
        camera.properties.push_back(MakeProperty("Camera.Projection", "Projection", MakeStringPropertyValue(EditorPropertyType::Enum, "Perspective")));
        camera.properties.push_back(MakeRangedProperty("Camera.FovY", "FOV Y", MakeNumberPropertyValue(EditorPropertyType::Float, 60.0), 1.0f, 179.0f, 0.1f, EditorPropertyFlag::Serialized | EditorPropertyFlag::Dirty));
        camera.properties.push_back(MakeRangedProperty("Camera.Near", "Near Plane", MakeNumberPropertyValue(EditorPropertyType::Float, 0.05), 0.001f, 10.0f, 0.001f, EditorPropertyFlag::Serialized | EditorPropertyFlag::Advanced));
        camera.properties.push_back(MakeRangedProperty("Camera.Far", "Far Plane", MakeNumberPropertyValue(EditorPropertyType::Float, 1000.0), 1.0f, 1000000.0f, 1.0f, EditorPropertyFlag::Serialized | EditorPropertyFlag::Advanced));
        camera.properties.push_back(MakeProperty("Camera.Primary", "Primary", MakeBoolPropertyValue(true)));
        camera.properties.push_back(MakeProperty("Camera.ReversedZ", "Reversed-Z", MakeBoolPropertyValue(true), EditorPropertyFlag::ReadOnly));
        frame.inspector.components.push_back(std::move(camera));

        EditorComponentInspector light{};
        light.componentType = "LightComponent";
        light.displayName = "Light";
        light.properties.push_back(MakeProperty("Light.Type", "Type", MakeStringPropertyValue(EditorPropertyType::Enum, "Directional")));
        light.properties.push_back(MakeRangedProperty("Light.Intensity", "Intensity", MakeNumberPropertyValue(EditorPropertyType::Float, 2.5), 0.0f, 100000.0f, 0.1f));
        light.properties.push_back(MakeRangedProperty("Light.Color", "Color", MakeNumberPropertyValue(EditorPropertyType::Color, 1.0, 0.96, 0.84, 1.0), 0.0f, 1.0f, 0.01f));
        light.properties.push_back(MakeRangedProperty("Light.Range", "Range", MakeNumberPropertyValue(EditorPropertyType::Float, 10.0), 0.0f, 100000.0f, 0.1f, EditorPropertyFlag::Serialized | EditorPropertyFlag::Advanced));
        light.properties.push_back(MakeRangedProperty("Light.InnerCone", "Inner Cone", MakeNumberPropertyValue(EditorPropertyType::Float, 20.0), 0.0f, 179.0f, 0.1f, EditorPropertyFlag::Serialized | EditorPropertyFlag::Advanced));
        light.properties.push_back(MakeRangedProperty("Light.OuterCone", "Outer Cone", MakeNumberPropertyValue(EditorPropertyType::Float, 35.0), 0.0f, 179.0f, 0.1f, EditorPropertyFlag::Serialized | EditorPropertyFlag::Advanced));
        frame.inspector.components.push_back(std::move(light));

        EditorComponentInspector render{};
        render.componentType = "MeshComponent";
        render.displayName = "Mesh";
        render.properties.push_back(MakeProperty("Mesh.Asset", "Mesh Asset", MakeStringPropertyValue(EditorPropertyType::AssetGuid, "mesh-camera-gizmo"), EditorPropertyFlag::Serialized | EditorPropertyFlag::RequiresRebuild));
        render.properties.push_back(MakeProperty("Mesh.Material", "Material", MakeStringPropertyValue(EditorPropertyType::AssetGuid, "mat-default"), EditorPropertyFlag::Serialized | EditorPropertyFlag::RequiresRebuild));
        render.properties.push_back(MakeProperty("Mesh.Visible", "Visible", MakeBoolPropertyValue(true)));
        render.properties.push_back(MakeProperty("Mesh.RequiresProxyRebuild", "Requires Proxy Rebuild", MakeBoolPropertyValue(false), EditorPropertyFlag::ReadOnly | EditorPropertyFlag::RequiresRebuild));
        frame.inspector.components.push_back(std::move(render));

        EditorComponentInspector bounds{};
        bounds.componentType = "BoundsComponent";
        bounds.displayName = "Bounds";
        bounds.properties.push_back(MakeRangedProperty("Bounds.LocalCenter", "Local Center", MakeNumberPropertyValue(EditorPropertyType::Vec3, 0.0, 0.0, 0.0), -100000.0f, 100000.0f, 0.1f, EditorPropertyFlag::Serialized | EditorPropertyFlag::RequiresRebuild));
        bounds.properties.push_back(MakeRangedProperty("Bounds.LocalExtents", "Local Extents", MakeNumberPropertyValue(EditorPropertyType::Vec3, 0.5, 0.5, 0.5), 0.0f, 100000.0f, 0.01f, EditorPropertyFlag::Serialized | EditorPropertyFlag::RequiresRebuild));
        bounds.properties.push_back(MakeRangedProperty("Bounds.SphereRadius", "Sphere Radius", MakeNumberPropertyValue(EditorPropertyType::Float, 0.8660254), 0.0f, 100000.0f, 0.01f, EditorPropertyFlag::ReadOnly));
        bounds.properties.push_back(MakeProperty("Bounds.Visible", "Visible", MakeBoolPropertyValue(true)));
        bounds.properties.push_back(MakeProperty("Bounds.Culled", "Culled", MakeBoolPropertyValue(false), EditorPropertyFlag::ReadOnly));
        bounds.properties.push_back(MakeProperty("Bounds.Revision", "Revision", MakeNumberPropertyValue(EditorPropertyType::Int, 1.0), EditorPropertyFlag::ReadOnly));
        frame.inspector.components.push_back(std::move(bounds));

        EditorComponentInspector destructible{};
        destructible.componentType = "DestructibleComponent";
        destructible.displayName = "Destructible";
        destructible.properties.push_back(MakeProperty("Destructible.Enabled", "Enabled", MakeBoolPropertyValue(true)));
        destructible.properties.push_back(MakeProperty("Destructible.Material", "Material", MakeStringPropertyValue(EditorPropertyType::Enum, "Concrete")));
        destructible.properties.push_back(MakeRangedProperty("Destructible.Density", "Density kg/m3", MakeNumberPropertyValue(EditorPropertyType::Float, 2400.0), 0.0f, 50000.0f, 1.0f));
        destructible.properties.push_back(MakeRangedProperty("Destructible.FractureResistance", "Fracture Resistance", MakeNumberPropertyValue(EditorPropertyType::Float, 1.0), 0.0f, 1000.0f, 0.01f));
        destructible.properties.push_back(MakeRangedProperty("Destructible.Resolution", "Voxel Resolution", MakeNumberPropertyValue(EditorPropertyType::Vec3, 16.0, 16.0, 16.0), 1.0f, 512.0f, 1.0f, EditorPropertyFlag::Serialized | EditorPropertyFlag::RequiresRebuild));
        destructible.properties.push_back(MakeRangedProperty("Destructible.MaxCollisionProxies", "Max Collision Proxies", MakeNumberPropertyValue(EditorPropertyType::Int, 128.0), 0.0f, 4096.0f, 1.0f, EditorPropertyFlag::Serialized | EditorPropertyFlag::RequiresRebuild));
        destructible.properties.push_back(MakeProperty("Destructible.UpdateBounds", "Update Bounds", MakeBoolPropertyValue(true), EditorPropertyFlag::Serialized | EditorPropertyFlag::RequiresRebuild));
        destructible.properties.push_back(MakeProperty("Destructible.GeneratedMesh", "Generated Mesh", MakeStringPropertyValue(EditorPropertyType::AssetGuid, ""), EditorPropertyFlag::ReadOnly | EditorPropertyFlag::Advanced));
        frame.inspector.components.push_back(std::move(destructible));

        EditorHierarchyNode scene = MakeEntityNode(1, 1, "Sandbox Scene", 0, false);
        scene.icon = "scene";
        scene.children.push_back(MakeEntityNode(2, 1, "CameraRig", 1, true));
        scene.children.push_back(MakeEntityNode(3, 1, "Directional Light", 1, false));
        EditorHierarchyNode world = MakeEntityNode(4, 1, "World", 1, false);
        world.children.push_back(MakeEntityNode(5, 1, "Terrain Root", 2, false));
        world.children.push_back(MakeEntityNode(6, 1, "Streaming Cell 0,0", 2, false));
        scene.children.push_back(std::move(world));
        frame.hierarchy.roots.push_back(std::move(scene));
        for (const EditorHierarchyNode& root : frame.hierarchy.roots)
        {
            CountHierarchyNode(root, frame.hierarchy.visibleNodeCount);
        }

        frame.assets.rootPath = "assets";
        frame.assets.currentPath = "assets";
        frame.assets.breadcrumbs = {"assets"};
        frame.assets.items.push_back({"folder-materials", "materials", "assets/materials", EditorAssetKind::Folder, 0, HashText("assets/materials"), true, false, false});
        frame.assets.items.push_back({"folder-meshes", "meshes", "assets/meshes", EditorAssetKind::Folder, 0, HashText("assets/meshes"), true, false, false});
        frame.assets.items.push_back({"scene-sandbox", "Sandbox.akscene", "assets/scenes/Sandbox.akscene", EditorAssetKind::Scene, 4096, HashText("Sandbox.akscene"), false, true, false});
        frame.assets.items.push_back({"mat-default", "DefaultMaterial.akmat", "assets/materials/DefaultMaterial.akmat", EditorAssetKind::Material, 2048, HashText("DefaultMaterial.akmat"), false, false, false});
        frame.assets.items.push_back({"pc-city", "CityScan.akpc", "assets/pointcloud/CityScan.akpc", EditorAssetKind::PointCloud, 33554432, HashText("CityScan.akpc"), false, false, false});
        frame.assets.items.push_back({"tex-missing", "MissingAlbedo.png", "assets/textures/MissingAlbedo.png", EditorAssetKind::Texture, 0, 0, false, false, true});

        frame.console.entries.push_back({EditorConsoleSeverity::Info, "Editor", "Editor UI model frame built", 12, 1});
        frame.console.entries.push_back({EditorConsoleSeverity::Warning, "Assets", "Texture asset has missing source file", 12, 1});
        frame.console.entries.push_back({EditorConsoleSeverity::Info, "Renderer", "GDI shell active, Vulkan/ImGui shell pending", 12, 1});
        frame.console.entries.push_back({EditorConsoleSeverity::Error, "Import", "Example import error route is visible in console model", 13, 1});

        frame.diagnostics.frameIndex = 12;
        frame.diagnostics.metrics.push_back({"editor.panels", "Panels", static_cast<double>(bridge.plan.panels.size()), EditorMetricUnit::Count, false});
        frame.diagnostics.metrics.push_back({"editor.search", "Search Records", static_cast<double>(bridge.searchIndex.records.size()), EditorMetricUnit::Count, false});
        frame.diagnostics.metrics.push_back({"assets.visible", "Visible Assets", static_cast<double>(frame.assets.items.size()), EditorMetricUnit::Count, false});
        frame.diagnostics.metrics.push_back({"frame.ui", "UI Frame", 0.28, EditorMetricUnit::Milliseconds, false});
        frame.diagnostics.metrics.push_back({"memory.editor", "Editor Model Memory", 48.0 * 1024.0, EditorMetricUnit::Bytes, false});
        frame.diagnostics.recentEvents.push_back("layout: AKLAYOUT 1 loaded");
        frame.diagnostics.recentEvents.push_back("selection: CameraRig active");
        frame.diagnostics.recentEvents.push_back("assets: 1 missing source detected");

        frame.searchIndex = BuildPanelModelSearchIndex(frame, bridge.searchIndex);
        return frame;
    }

    EditorPanelModelDiagnostics ValidateEditorPanelModelFrame(const EditorPanelModelFrame& frame)
    {
        EditorPanelModelDiagnostics diagnostics{};
        diagnostics.hierarchyNodeCount = frame.hierarchy.visibleNodeCount;
        diagnostics.componentCount = frame.inspector.components.size();
        diagnostics.assetCount = frame.assets.items.size();
        diagnostics.metricCount = frame.diagnostics.metrics.size();
        diagnostics.searchRecordCount = frame.searchIndex.records.size();

        std::unordered_set<std::string> propertyPaths;
        bool propertyPathsValid = true;
        for (const EditorComponentInspector& component : frame.inspector.components)
        {
            for (const EditorPropertyDesc& property : component.properties)
            {
                ++diagnostics.propertyCount;
                if (HasFlag(property.flags, EditorPropertyFlag::Dirty))
                {
                    ++diagnostics.dirtyPropertyCount;
                }
                if (HasFlag(property.flags, EditorPropertyFlag::ReadOnly))
                {
                    ++diagnostics.readOnlyPropertyCount;
                }
                if (property.hasRange)
                {
                    ++diagnostics.rangedPropertyCount;
                }
                if (property.hasStep)
                {
                    ++diagnostics.steppedPropertyCount;
                }
                if (property.path.empty() || !propertyPaths.insert(property.path).second)
                {
                    propertyPathsValid = false;
                }
            }
        }

        for (const EditorAssetItem& asset : frame.assets.items)
        {
            if (asset.missing)
            {
                ++diagnostics.missingAssetCount;
            }
        }
        const bool assetSelectionValid = frame.assets.items.empty()
            ? frame.assets.selectedIndex == 0u
            : frame.assets.selectedIndex < frame.assets.items.size();

        for (const EditorConsoleEntry& entry : frame.console.entries)
        {
            if (entry.severity == EditorConsoleSeverity::Error)
            {
                ++diagnostics.consoleErrorCount;
            }
        }

        diagnostics.ok = diagnostics.hierarchyNodeCount >= 5
            && diagnostics.componentCount >= 7
            && diagnostics.propertyCount >= 35
            && diagnostics.rangedPropertyCount >= 18
            && diagnostics.steppedPropertyCount >= 18
            && diagnostics.readOnlyPropertyCount >= 6
            && diagnostics.assetCount >= 5
            && assetSelectionValid
            && diagnostics.metricCount >= 4
            && diagnostics.searchRecordCount >= 20
            && propertyPathsValid;

        std::ostringstream out;
        out << "panel-model hierarchy=" << diagnostics.hierarchyNodeCount
            << " components=" << diagnostics.componentCount
            << " properties=" << diagnostics.propertyCount
            << " dirty=" << diagnostics.dirtyPropertyCount
            << " ranges=" << diagnostics.rangedPropertyCount
            << " steps=" << diagnostics.steppedPropertyCount
            << " readonly=" << diagnostics.readOnlyPropertyCount
            << " assets=" << diagnostics.assetCount
            << " assetSelectionValid=" << (assetSelectionValid ? "true" : "false")
            << " missingAssets=" << diagnostics.missingAssetCount
            << " consoleErrors=" << diagnostics.consoleErrorCount
            << " metrics=" << diagnostics.metricCount
            << " search=" << diagnostics.searchRecordCount
            << " ok=" << (diagnostics.ok ? "true" : "false");
        diagnostics.summary = out.str();
        return diagnostics;
    }

    EditorSearchIndex BuildPanelModelSearchIndex(const EditorPanelModelFrame& frame, const EditorSearchIndex& baseIndex)
    {
        EditorSearchIndex index = baseIndex;
        for (const EditorComponentInspector& component : frame.inspector.components)
        {
            AddSearchRecord(index, EditorSearchResultKind::Setting, component.displayName, "Inspector component", "component:" + component.componentType, 0.72f);
            for (const EditorPropertyDesc& property : component.properties)
            {
                if (property.path.empty())
                {
                    continue;
                }
                AddSearchRecord(index, EditorSearchResultKind::Setting, property.label, property.path, "property:" + property.path, HasFlag(property.flags, EditorPropertyFlag::Dirty) ? 0.95f : 0.68f);
            }
        }
        for (const EditorAssetItem& asset : frame.assets.items)
        {
            if (asset.guid.empty())
            {
                continue;
            }
            AddSearchRecord(index, EditorSearchResultKind::Asset, asset.name, std::string(ToString(asset.kind)) + "  " + asset.logicalPath, "asset:" + asset.guid, asset.missing ? 0.35f : 0.8f);
        }
        for (const EditorMetric& metric : frame.diagnostics.metrics)
        {
            AddSearchRecord(index, EditorSearchResultKind::Setting, metric.label, "Diagnostics metric", "metric:" + metric.key, metric.warning ? 0.7f : 0.45f);
        }
        return index;
    }

    std::string FormatEditorProperty(const EditorPropertyDesc& property)
    {
        std::ostringstream out;
        out << property.path << " label=" << property.label
            << " type=" << ToString(property.value.type)
            << " value=" << property.value.text
            << " flags=0x" << std::hex << static_cast<u32>(property.flags) << std::dec;
        if (property.hasRange)
        {
            out << " range=" << property.minValue << ".." << property.maxValue;
        }
        if (property.hasStep)
        {
            out << " step=" << property.stepValue;
        }
        return out.str();
    }

    std::string FormatEditorHierarchyNode(const EditorHierarchyNode& node)
    {
        std::ostringstream out;
        out << node.stableId << " name=" << node.name
            << " depth=" << node.depth
            << " children=" << node.children.size()
            << " selected=" << (node.selected ? "true" : "false");
        return out.str();
    }

    std::string FormatEditorAssetItem(const EditorAssetItem& item)
    {
        std::ostringstream out;
        out << item.guid << " name=" << item.name
            << " kind=" << ToString(item.kind)
            << " path=" << item.logicalPath
            << " bytes=" << item.sizeBytes
            << " missing=" << (item.missing ? "true" : "false");
        return out.str();
    }

    std::string FormatEditorConsoleEntry(const EditorConsoleEntry& entry)
    {
        std::ostringstream out;
        out << '[' << ToString(entry.severity) << "] " << entry.channel << ": " << entry.message << " frame=" << entry.frame;
        return out.str();
    }

    std::string FormatEditorMetric(const EditorMetric& metric)
    {
        std::ostringstream out;
        out << metric.key << '=' << metric.value << ToString(metric.unit) << " label=" << metric.label;
        return out.str();
    }

    std::string FormatEditorPanelModelDiagnostics(const EditorPanelModelDiagnostics& diagnostics)
    {
        return diagnostics.summary;
    }
}
