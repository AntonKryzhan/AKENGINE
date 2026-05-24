#pragma once

#include <AK/Core/Types.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace AK
{
    enum class BuildMode : u32
    {
        Debug = 0,
        Development = 1,
        Release = 2,
        Shipping = 3
    };

    enum class AngleUnit : u32
    {
        Radians = 0,
        Degrees = 1
    };

    enum class Handedness : u32
    {
        RightHanded = 0,
        LeftHanded = 1
    };

    enum class UpAxis : u32
    {
        Y = 0,
        Z = 1
    };

    enum class MatrixLayout : u32
    {
        ColumnMajor = 0,
        RowMajor = 1
    };

    enum class ClipDepthRange : u32
    {
        ZeroToOne = 0,
        MinusOneToOne = 1
    };

    enum class FrontFaceWinding : u32
    {
        CounterClockwise = 0,
        Clockwise = 1
    };

    struct BuildModePolicy final
    {
        BuildMode mode = BuildMode::Development;
        bool assertions = true;
        bool validation = true;
        bool profiling = true;
        bool editorAllowed = true;
        bool hotReloadAllowed = true;
        bool verboseLogging = true;
        bool crashDumps = true;
    };

    struct UnitsPolicy final
    {
        double metersPerUnit = 1.0;
        bool secondsForTime = true;
        bool kilogramsForMass = true;
        AngleUnit internalAngles = AngleUnit::Radians;
        bool degreesAllowedInUiOnly = true;
    };

    struct CoordinatePolicy final
    {
        Handedness handedness = Handedness::RightHanded;
        UpAxis upAxis = UpAxis::Y;
        MatrixLayout matrixLayout = MatrixLayout::ColumnMajor;
        ClipDepthRange clipDepthRange = ClipDepthRange::ZeroToOne;
        FrontFaceWinding frontFace = FrontFaceWinding::CounterClockwise;
        bool cameraRelativeGpuPositions = true;
        bool reversedZDepth = true;
    };

    struct GpuSynchronizationPolicy final
    {
        u32 framesInFlight = 3;
        u32 deferredReleaseFrames = 3;
        bool duplicatePerFrameWritableResources = true;
        bool allowHotFrameReadback = false;
        bool allowDeviceWaitIdleInFrame = false;
        bool preferTimelineSemaphores = true;
        bool asyncUploadQueues = true;
    };

    struct ShaderPermutationBudget final
    {
        u32 maxKeywordsPerShader = 16;
        u64 maxPermutationsPerShader = 1024;
        u64 maxCachedPipelines = 65536;
        bool requireFallbackVariant = true;
        bool allowRuntimeCompilationInShipping = false;
    };

    struct ShaderFeatureDesc final
    {
        std::string name;
        u32 variants = 2;
        bool shippingAllowed = true;
        bool runtimeOnly = false;
    };

    struct PolicyValidationReport final
    {
        bool ok = true;
        u32 warnings = 0;
        std::vector<std::string> messages;
        std::string summary;
    };

    struct ShaderPermutationReport final
    {
        u32 features = 0;
        u32 shippingDisabledFeatures = 0;
        u64 permutations = 1;
        u64 cacheBudget = 0;
        bool withinBudget = true;
        bool fallbackReady = true;
        bool runtimeCompilationRisk = false;
        std::vector<std::string> messages;
        std::string summary;
    };

    class ShaderPermutationPlan final
    {
    public:
        bool RegisterFeature(ShaderFeatureDesc feature);
        void Clear();

        [[nodiscard]] ShaderPermutationReport Analyze(const ShaderPermutationBudget& budget, BuildMode mode) const;
        [[nodiscard]] const std::vector<ShaderFeatureDesc>& Features() const;

    private:
        std::vector<ShaderFeatureDesc> mFeatures;
    };

    struct EnginePolicySet final
    {
        BuildModePolicy build{};
        UnitsPolicy units{};
        CoordinatePolicy coordinates{};
        GpuSynchronizationPolicy gpuSync{};
        ShaderPermutationBudget shaderBudget{};
    };

    struct FutureLimitProbeResult final
    {
        bool ok = false;
        EnginePolicySet policies{};
        PolicyValidationReport buildReport{};
        PolicyValidationReport unitsReport{};
        PolicyValidationReport coordinateReport{};
        PolicyValidationReport gpuSyncReport{};
        ShaderPermutationReport safeShaderReport{};
        ShaderPermutationReport rejectedShaderReport{};
        std::string summary;
    };

    BuildMode DetectBuildMode();
    BuildModePolicy MakeBuildModePolicy(BuildMode mode);
    EnginePolicySet MakeDefaultEnginePolicySet(BuildMode mode = DetectBuildMode());

    PolicyValidationReport ValidateBuildModePolicy(const BuildModePolicy& policy);
    PolicyValidationReport ValidateUnitsPolicy(const UnitsPolicy& policy);
    PolicyValidationReport ValidateCoordinatePolicy(const CoordinatePolicy& policy);
    PolicyValidationReport ValidateGpuSynchronizationPolicy(const GpuSynchronizationPolicy& policy);

    ShaderPermutationPlan BuildDefaultShaderPermutationPlan();
    ShaderPermutationPlan BuildPermutationExplosionTestPlan(u32 binaryFeatureCount);

    FutureLimitProbeResult BuildFutureLimitProbe();
    std::string BuildFutureLimitProbeSummary();

    const char* ToString(BuildMode mode);
    const char* ToString(AngleUnit unit);
    const char* ToString(Handedness handedness);
    const char* ToString(UpAxis axis);
    const char* ToString(MatrixLayout layout);
    const char* ToString(ClipDepthRange range);
    const char* ToString(FrontFaceWinding winding);
    std::string ToDebugString(const PolicyValidationReport& report);
    std::string ToDebugString(const ShaderPermutationReport& report);
}
