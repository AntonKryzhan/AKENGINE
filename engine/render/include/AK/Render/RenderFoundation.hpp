#pragma once

#include <AK/Core/Types.hpp>
#include <AK/Math/Geometry.hpp>
#include <AK/World/WorldCoordinates.hpp>

#include <string>
#include <vector>

namespace AK
{
    enum class DepthConvention : u32
    {
        StandardZ = 0,
        ReversedZ = 1
    };

    struct DepthPolicy
    {
        DepthConvention convention = DepthConvention::ReversedZ;
        float nearPlane = 0.05f;
        float farPlane = 100000.0f;
        bool infiniteFarPlane = true;
        bool floatingPointDepth = true;
    };

    struct DepthPrecisionReport
    {
        float nearPlane = 0.05f;
        float farPlane = 100000.0f;
        double farNearRatio = 2000000.0;
        bool reversedZ = true;
        bool infiniteFarPlane = true;
        bool floatingPointDepth = true;
        bool nearPlaneTooSmall = false;
        bool farNearRatioRisk = false;
        bool standardDepthRisk = false;
        bool valid = true;
        std::string recommendation;
    };

    struct CameraRelativeRenderPosition
    {
        Vec3 value{};
        double distanceMeters = 0.0;
        double estimatedFloatStepMeters = 0.0;
        bool finite = true;
        bool precisionRisk = false;
    };

    enum class RenderResourceUsage : u32
    {
        Unknown = 0,
        ColorAttachment = 1,
        DepthAttachment = 2,
        ShaderRead = 3,
        TransferSrc = 4,
        TransferDst = 5,
        Present = 6
    };

    struct RenderGraphResourceDesc
    {
        std::string name;
        RenderResourceUsage initialUsage = RenderResourceUsage::Unknown;
        RenderResourceUsage finalUsage = RenderResourceUsage::Unknown;
        bool transient = true;
        bool imported = false;
    };

    struct RenderGraphPassDesc
    {
        std::string name;
        std::vector<std::string> reads;
        std::vector<std::string> writes;
        bool asyncCompute = false;
        bool present = false;
    };

    struct RenderGraphStats
    {
        u32 resources = 0;
        u32 passes = 0;
        u32 transientResources = 0;
        u32 importedResources = 0;
        u32 asyncComputePasses = 0;
        u32 presentPasses = 0;
        u32 barriersEstimated = 0;
        u32 validationWarnings = 0;
    };

    class RenderGraphPlan final
    {
    public:
        bool AddResource(RenderGraphResourceDesc resource);
        bool AddPass(RenderGraphPassDesc pass);
        RenderGraphStats Build() const;
        const std::vector<RenderGraphResourceDesc>& Resources() const;
        const std::vector<RenderGraphPassDesc>& Passes() const;

    private:
        const RenderGraphResourceDesc* FindResource(const std::string& name) const;

        std::vector<RenderGraphResourceDesc> mResources;
        std::vector<RenderGraphPassDesc> mPasses;
    };

    struct RenderFoundationProbe
    {
        DepthPolicy depthPolicy{};
        DepthPrecisionReport depthReport{};
        RenderGraphStats graphStats{};
        CameraRelativeRenderPosition cameraRelative{};
        std::string summary;
    };

    DepthPolicy MakeDefaultDepthPolicy();
    DepthPrecisionReport AnalyzeDepthPolicy(const DepthPolicy& policy);
    float ProjectViewDepthToDeviceZ(float viewDepthMeters, const DepthPolicy& policy);
    CameraRelativeRenderPosition MakeCameraRelativeRenderPosition(WorldPosition position, WorldPosition camera, const LargeWorldConfig& config = {});

    RenderGraphPlan BuildDefaultRenderGraphPlan();
    RenderFoundationProbe BuildRenderFoundationProbe();

    const char* ToString(DepthConvention convention);
    const char* ToString(RenderResourceUsage usage);
    std::string ToDebugString(const DepthPrecisionReport& report);
    std::string ToDebugString(const RenderGraphStats& stats);
}
