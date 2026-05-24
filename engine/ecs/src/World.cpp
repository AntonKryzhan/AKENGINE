#include <AK/ECS/World.hpp>

#include <algorithm>
#include <utility>

namespace AK
{
    EntityId World::AllocateEntityId()
    {
        u32 index = 0;
        if (!mFreeEntityIndices.empty())
        {
            index = mFreeEntityIndices.back();
            mFreeEntityIndices.pop_back();
        }
        else
        {
            index = mNextEntityIndex++;
        }

        u32& generation = mGenerations[index];
        if (generation == 0)
        {
            generation = 1;
        }

        return EntityId{index, generation};
    }

    void World::ReleaseEntityId(EntityId entity)
    {
        if (!entity.IsValid())
        {
            return;
        }

        u32& generation = mGenerations[entity.index];
        generation = std::max<u32>(1u, generation + 1u);
        mFreeEntityIndices.push_back(entity.index);
    }

    EntityId World::CreateEntity(std::string name)
    {
        const EntityId id = AllocateEntityId();
        if (name.empty())
        {
            name = "Entity_" + std::to_string(id.index);
        }

        mEntities.push_back(EntityRecord{id, std::move(name)});
        return id;
    }

    bool World::DestroyEntity(EntityId entity)
    {
        if (!IsAlive(entity))
        {
            return false;
        }

        const auto before = mEntities.size();
        mEntities.erase(
            std::remove_if(mEntities.begin(), mEntities.end(), [entity](const EntityRecord& record)
            {
                return record.id == entity;
            }),
            mEntities.end());

        mTransforms.erase(entity);
        mBounds.erase(entity);
        mWorldPositions.erase(entity);
        mCameras.erase(entity);
        mLights.erase(entity);
        mMeshes.erase(entity);
        ReleaseEntityId(entity);
        return before != mEntities.size();
    }

    bool World::RenameEntity(EntityId entity, std::string name)
    {
        if (name.empty() || !IsAlive(entity))
        {
            return false;
        }

        for (EntityRecord& record : mEntities)
        {
            if (record.id == entity)
            {
                record.name = std::move(name);
                return true;
            }
        }

        return false;
    }

    bool World::IsAlive(EntityId entity) const
    {
        if (!entity.IsValid())
        {
            return false;
        }

        const auto generationIt = mGenerations.find(entity.index);
        if (generationIt == mGenerations.end() || generationIt->second != entity.generation)
        {
            return false;
        }

        return std::any_of(mEntities.begin(), mEntities.end(), [entity](const EntityRecord& record)
        {
            return record.id == entity;
        });
    }

    TransformComponent& World::AddTransform(EntityId entity)
    {
        return mTransforms[entity];
    }

    bool World::HasTransform(EntityId entity) const
    {
        return mTransforms.find(entity) != mTransforms.end();
    }

    TransformComponent* World::GetTransform(EntityId entity)
    {
        const auto it = mTransforms.find(entity);
        return it == mTransforms.end() ? nullptr : &it->second;
    }

    const TransformComponent* World::GetTransform(EntityId entity) const
    {
        const auto it = mTransforms.find(entity);
        return it == mTransforms.end() ? nullptr : &it->second;
    }

    BoundsComponent& World::AddBounds(EntityId entity)
    {
        return mBounds[entity];
    }

    bool World::RemoveBounds(EntityId entity)
    {
        return mBounds.erase(entity) > 0;
    }

    bool World::HasBounds(EntityId entity) const
    {
        return mBounds.find(entity) != mBounds.end();
    }

    BoundsComponent* World::GetBounds(EntityId entity)
    {
        const auto it = mBounds.find(entity);
        return it == mBounds.end() ? nullptr : &it->second;
    }

    const BoundsComponent* World::GetBounds(EntityId entity) const
    {
        const auto it = mBounds.find(entity);
        return it == mBounds.end() ? nullptr : &it->second;
    }

    WorldPositionComponent& World::AddWorldPosition(EntityId entity)
    {
        return mWorldPositions[entity];
    }

    bool World::RemoveWorldPosition(EntityId entity)
    {
        return mWorldPositions.erase(entity) > 0;
    }

    bool World::HasWorldPosition(EntityId entity) const
    {
        return mWorldPositions.find(entity) != mWorldPositions.end();
    }

    WorldPositionComponent* World::GetWorldPosition(EntityId entity)
    {
        const auto it = mWorldPositions.find(entity);
        return it == mWorldPositions.end() ? nullptr : &it->second;
    }

    const WorldPositionComponent* World::GetWorldPosition(EntityId entity) const
    {
        const auto it = mWorldPositions.find(entity);
        return it == mWorldPositions.end() ? nullptr : &it->second;
    }

    CameraComponent& World::AddCamera(EntityId entity)
    {
        return mCameras[entity];
    }

    bool World::RemoveCamera(EntityId entity)
    {
        return mCameras.erase(entity) > 0;
    }

    bool World::HasCamera(EntityId entity) const
    {
        return mCameras.find(entity) != mCameras.end();
    }

    CameraComponent* World::GetCamera(EntityId entity)
    {
        const auto it = mCameras.find(entity);
        return it == mCameras.end() ? nullptr : &it->second;
    }

    const CameraComponent* World::GetCamera(EntityId entity) const
    {
        const auto it = mCameras.find(entity);
        return it == mCameras.end() ? nullptr : &it->second;
    }

    LightComponent& World::AddLight(EntityId entity)
    {
        return mLights[entity];
    }

    bool World::RemoveLight(EntityId entity)
    {
        return mLights.erase(entity) > 0;
    }

    bool World::HasLight(EntityId entity) const
    {
        return mLights.find(entity) != mLights.end();
    }

    LightComponent* World::GetLight(EntityId entity)
    {
        const auto it = mLights.find(entity);
        return it == mLights.end() ? nullptr : &it->second;
    }

    const LightComponent* World::GetLight(EntityId entity) const
    {
        const auto it = mLights.find(entity);
        return it == mLights.end() ? nullptr : &it->second;
    }

    MeshComponent& World::AddMesh(EntityId entity)
    {
        return mMeshes[entity];
    }

    bool World::RemoveMesh(EntityId entity)
    {
        return mMeshes.erase(entity) > 0;
    }

    bool World::HasMesh(EntityId entity) const
    {
        return mMeshes.find(entity) != mMeshes.end();
    }

    MeshComponent* World::GetMesh(EntityId entity)
    {
        const auto it = mMeshes.find(entity);
        return it == mMeshes.end() ? nullptr : &it->second;
    }

    const MeshComponent* World::GetMesh(EntityId entity) const
    {
        const auto it = mMeshes.find(entity);
        return it == mMeshes.end() ? nullptr : &it->second;
    }

    std::size_t World::TransformCount() const
    {
        return mTransforms.size();
    }

    std::size_t World::BoundsCount() const
    {
        return mBounds.size();
    }

    std::size_t World::WorldPositionCount() const
    {
        return mWorldPositions.size();
    }

    std::size_t World::CameraCount() const
    {
        return mCameras.size();
    }

    std::size_t World::LightCount() const
    {
        return mLights.size();
    }

    std::size_t World::MeshCount() const
    {
        return mMeshes.size();
    }

    const std::vector<EntityRecord>& World::Entities() const
    {
        return mEntities;
    }

    void World::Clear()
    {
        mNextEntityIndex = 1;
        mFreeEntityIndices.clear();
        mGenerations.clear();
        mEntities.clear();
        mTransforms.clear();
        mBounds.clear();
        mWorldPositions.clear();
        mCameras.clear();
        mLights.clear();
        mMeshes.clear();
    }
}
