#include <AK/Scene/Scene.hpp>

#include <AK/Core/Log.hpp>
#include <AK/Core/Path.hpp>

#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>

namespace AK
{
    namespace
    {
        struct FileEntityRef final
        {
            u32 index = 0;
            u32 generation = 0;
        };

        std::string ToKey(FileEntityRef ref)
        {
            return std::to_string(ref.index) + ":" + std::to_string(ref.generation);
        }

        FileEntityRef ReadEntityRef(std::istream& in, int version)
        {
            FileEntityRef ref{};
            in >> ref.index;
            if (version >= 5)
            {
                in >> ref.generation;
            }
            return ref;
        }

        void WriteEntityRef(std::ostream& out, EntityId entity)
        {
            out << entity.index << ' ' << entity.generation;
        }

        const char* LightTypeToToken(LightType type)
        {
            switch (type)
            {
                case LightType::Directional:
                    return "directional";
                case LightType::Point:
                    return "point";
                case LightType::Spot:
                    return "spot";
                default:
                    return "directional";
            }
        }

        LightType LightTypeFromToken(const std::string& token)
        {
            if (token == "point")
            {
                return LightType::Point;
            }
            if (token == "spot")
            {
                return LightType::Spot;
            }
            return LightType::Directional;
        }
    }

    Scene::Scene(std::string name)
        : mName(std::move(name))
    {
    }

    const std::string& Scene::Name() const
    {
        return mName;
    }

    World& Scene::GetWorld()
    {
        return mWorld;
    }

    const World& Scene::GetWorld() const
    {
        return mWorld;
    }

    bool Scene::SaveToFile(const std::filesystem::path& path) const
    {
        std::ostringstream file;
        file << std::setprecision(17);
        file << "AKSCENE 6\n";
        file << "name " << std::quoted(mName) << "\n";

        for (const EntityRecord& entity : mWorld.Entities())
        {
            const TransformComponent* transform = mWorld.GetTransform(entity.id);
            if (!transform)
            {
                continue;
            }

            file << "entity ";
            WriteEntityRef(file, entity.id);
            file << ' '
                 << std::quoted(entity.name) << ' '
                 << transform->position.x << ' ' << transform->position.y << ' ' << transform->position.z << ' '
                 << transform->rotation.x << ' ' << transform->rotation.y << ' ' << transform->rotation.z << ' '
                 << transform->scale.x << ' ' << transform->scale.y << ' ' << transform->scale.z << '\n';
        }

        for (const EntityRecord& entity : mWorld.Entities())
        {
            if (const WorldPositionComponent* worldPosition = mWorld.GetWorldPosition(entity.id))
            {
                file << "worldpos ";
                WriteEntityRef(file, entity.id);
                file << ' '
                     << worldPosition->position.cell.x << ' '
                     << worldPosition->position.cell.y << ' '
                     << worldPosition->position.cell.z << ' '
                     << worldPosition->position.localX << ' '
                     << worldPosition->position.localY << ' '
                     << worldPosition->position.localZ << ' '
                     << (worldPosition->authoritative ? 1 : 0) << '\n';
            }

            if (const CameraComponent* camera = mWorld.GetCamera(entity.id))
            {
                file << "camera ";
                WriteEntityRef(file, entity.id);
                file << ' '
                     << camera->verticalFovDegrees << ' '
                     << camera->nearPlane << ' '
                     << camera->farPlane << ' '
                     << (camera->primary ? 1 : 0) << '\n';
            }

            if (const LightComponent* light = mWorld.GetLight(entity.id))
            {
                file << "light ";
                WriteEntityRef(file, entity.id);
                file << ' '
                     << LightTypeToToken(light->type) << ' '
                     << light->intensity << ' '
                     << light->color.x << ' ' << light->color.y << ' ' << light->color.z << ' '
                     << light->range << ' '
                     << light->innerConeDegrees << ' '
                     << light->outerConeDegrees << '\n';
            }

            if (const MeshComponent* mesh = mWorld.GetMesh(entity.id))
            {
                file << "mesh ";
                WriteEntityRef(file, entity.id);
                file << ' '
                     << std::quoted(mesh->mesh) << ' '
                     << std::quoted(mesh->material) << '\n';
            }

            if (const BoundsComponent* bounds = mWorld.GetBounds(entity.id))
            {
                file << "bounds ";
                WriteEntityRef(file, entity.id);
                file << ' '
                     << bounds->localBounds.min.x << ' '
                     << bounds->localBounds.min.y << ' '
                     << bounds->localBounds.min.z << ' '
                     << bounds->localBounds.max.x << ' '
                     << bounds->localBounds.max.y << ' '
                     << bounds->localBounds.max.z << '\n';
            }
        }

        const Result<void> writeResult = AtomicWriteTextFile(path, file.str());
        if (!writeResult)
        {
            LogError("Failed to write scene file: " + writeResult.GetError().message);
            return false;
        }

        return true;
    }

    bool Scene::LoadFromFile(const std::filesystem::path& path)
    {
        std::ifstream file(path);
        if (!file)
        {
            LogError("Failed to read scene file: " + path.string());
            return false;
        }

        std::string magic;
        int version = 0;
        file >> magic >> version;

        if (magic != "AKSCENE" || version < 1 || version > 6)
        {
            LogError("Unsupported scene file format: " + path.string());
            return false;
        }

        mWorld.Clear();
        std::unordered_map<std::string, EntityId> remap;

        std::string command;
        while (file >> command)
        {
            if (command == "name")
            {
                if (version >= 2)
                {
                    file >> std::quoted(mName);
                }
                else
                {
                    file >> mName;
                }
            }
            else if (command == "entity")
            {
                const FileEntityRef fileId = ReadEntityRef(file, version);
                std::string name;
                TransformComponent transform{};

                if (version >= 2)
                {
                    file >> std::quoted(name);
                }
                else
                {
                    file >> name;
                }

                file >> transform.position.x >> transform.position.y >> transform.position.z
                     >> transform.rotation.x >> transform.rotation.y >> transform.rotation.z
                     >> transform.scale.x >> transform.scale.y >> transform.scale.z;

                const EntityId entity = mWorld.CreateEntity(name);
                mWorld.AddTransform(entity) = transform;
                remap[ToKey(fileId)] = entity;
            }
            else if (command == "worldpos" && version >= 4)
            {
                const FileEntityRef fileId = ReadEntityRef(file, version);
                WorldPositionComponent worldPosition{};
                int authoritative = 1;
                file >> worldPosition.position.cell.x
                     >> worldPosition.position.cell.y
                     >> worldPosition.position.cell.z
                     >> worldPosition.position.localX
                     >> worldPosition.position.localY
                     >> worldPosition.position.localZ
                     >> authoritative;
                worldPosition.position = NormalizeWorldPosition(worldPosition.position);
                worldPosition.authoritative = authoritative != 0;

                const auto it = remap.find(ToKey(fileId));
                if (it != remap.end())
                {
                    mWorld.AddWorldPosition(it->second) = worldPosition;
                }
            }
            else if (command == "camera" && version >= 3)
            {
                const FileEntityRef fileId = ReadEntityRef(file, version);
                CameraComponent camera{};
                int primary = 0;
                file >> camera.verticalFovDegrees >> camera.nearPlane >> camera.farPlane >> primary;
                camera.primary = primary != 0;

                const auto it = remap.find(ToKey(fileId));
                if (it != remap.end())
                {
                    mWorld.AddCamera(it->second) = camera;
                }
            }
            else if (command == "light" && version >= 3)
            {
                const FileEntityRef fileId = ReadEntityRef(file, version);
                std::string type;
                LightComponent light{};
                file >> type
                     >> light.intensity
                     >> light.color.x >> light.color.y >> light.color.z
                     >> light.range
                     >> light.innerConeDegrees
                     >> light.outerConeDegrees;
                light.type = LightTypeFromToken(type);

                const auto it = remap.find(ToKey(fileId));
                if (it != remap.end())
                {
                    mWorld.AddLight(it->second) = light;
                }
            }
            else if (command == "mesh" && version >= 3)
            {
                const FileEntityRef fileId = ReadEntityRef(file, version);
                MeshComponent mesh{};
                file >> std::quoted(mesh.mesh) >> std::quoted(mesh.material);

                const auto it = remap.find(ToKey(fileId));
                if (it != remap.end())
                {
                    mWorld.AddMesh(it->second) = mesh;
                }
            }
            else if (command == "bounds" && version >= 6)
            {
                const FileEntityRef fileId = ReadEntityRef(file, version);
                BoundsComponent bounds{};
                file >> bounds.localBounds.min.x
                     >> bounds.localBounds.min.y
                     >> bounds.localBounds.min.z
                     >> bounds.localBounds.max.x
                     >> bounds.localBounds.max.y
                     >> bounds.localBounds.max.z;
                bounds.localBounds = MakeAABB3(bounds.localBounds.min, bounds.localBounds.max);
                bounds.dirtyFlags = BoundsDirty_All;

                const auto it = remap.find(ToKey(fileId));
                if (it != remap.end())
                {
                    mWorld.AddBounds(it->second) = bounds;
                }
            }
            else
            {
                std::string restOfLine;
                std::getline(file, restOfLine);
            }
        }

        return true;
    }
}
