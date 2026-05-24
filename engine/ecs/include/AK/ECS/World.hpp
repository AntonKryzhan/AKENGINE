#pragma once

#include <AK/ECS/Components.hpp>
#include <AK/ECS/EntityId.hpp>

#include <string>
#include <unordered_map>
#include <vector>

namespace AK
{
    struct EntityRecord
    {
        EntityId id = InvalidEntity;
        std::string name;
    };

    class World final
    {
    public:
        EntityId CreateEntity(std::string name = {});
        bool DestroyEntity(EntityId entity);
        bool RenameEntity(EntityId entity, std::string name);
        bool IsAlive(EntityId entity) const;

        TransformComponent& AddTransform(EntityId entity);
        bool HasTransform(EntityId entity) const;
        TransformComponent* GetTransform(EntityId entity);
        const TransformComponent* GetTransform(EntityId entity) const;

        BoundsComponent& AddBounds(EntityId entity);
        bool RemoveBounds(EntityId entity);
        bool HasBounds(EntityId entity) const;
        BoundsComponent* GetBounds(EntityId entity);
        const BoundsComponent* GetBounds(EntityId entity) const;

        WorldPositionComponent& AddWorldPosition(EntityId entity);
        bool RemoveWorldPosition(EntityId entity);
        bool HasWorldPosition(EntityId entity) const;
        WorldPositionComponent* GetWorldPosition(EntityId entity);
        const WorldPositionComponent* GetWorldPosition(EntityId entity) const;

        CameraComponent& AddCamera(EntityId entity);
        bool RemoveCamera(EntityId entity);
        bool HasCamera(EntityId entity) const;
        CameraComponent* GetCamera(EntityId entity);
        const CameraComponent* GetCamera(EntityId entity) const;

        LightComponent& AddLight(EntityId entity);
        bool RemoveLight(EntityId entity);
        bool HasLight(EntityId entity) const;
        LightComponent* GetLight(EntityId entity);
        const LightComponent* GetLight(EntityId entity) const;

        MeshComponent& AddMesh(EntityId entity);
        bool RemoveMesh(EntityId entity);
        bool HasMesh(EntityId entity) const;
        MeshComponent* GetMesh(EntityId entity);
        const MeshComponent* GetMesh(EntityId entity) const;

        std::size_t TransformCount() const;
        std::size_t BoundsCount() const;
        std::size_t WorldPositionCount() const;
        std::size_t CameraCount() const;
        std::size_t LightCount() const;
        std::size_t MeshCount() const;

        const std::vector<EntityRecord>& Entities() const;
        void Clear();

    private:
        EntityId AllocateEntityId();
        void ReleaseEntityId(EntityId entity);

        u32 mNextEntityIndex = 1;
        std::vector<u32> mFreeEntityIndices;
        std::unordered_map<u32, u32> mGenerations;
        std::vector<EntityRecord> mEntities;
        std::unordered_map<EntityId, TransformComponent> mTransforms;
        std::unordered_map<EntityId, BoundsComponent> mBounds;
        std::unordered_map<EntityId, WorldPositionComponent> mWorldPositions;
        std::unordered_map<EntityId, CameraComponent> mCameras;
        std::unordered_map<EntityId, LightComponent> mLights;
        std::unordered_map<EntityId, MeshComponent> mMeshes;
    };
}
