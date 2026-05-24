#pragma once

#include <AK/Core/Types.hpp>
#include <AK/Texture/TextureSet.hpp>

#include <string>
#include <vector>

namespace AK
{
    enum class NtcCompressionProfile : u32
    {
        NTC_0_2,
        NTC_0_5,
        NTC_1_0,
        NTC_2_25,
        Custom
    };

    enum class NtcRuntimeMode : u32
    {
        DisabledFallback,
        InferenceOnLoadToBCn,
        InferenceOnSample,
        Hybrid
    };

    enum class NtcIntegrationStage : u32
    {
        Foundation,
        CookerSdkBridge,
        VulkanFeatureDetection,
        ShaderDecodePath,
        TemporalFiltering,
        MaterialPolicy
    };

    struct NtcProfileDesc
    {
        NtcCompressionProfile profile = NtcCompressionProfile::NTC_0_2;
        double bitsPerPixelPerChannel = 0.2;
        u32 rootG0Resolution = 1024;
        u32 g0Channels = 8;
        u32 g0Bits = 2;
        u32 g1Channels = 12;
        u32 g1Bits = 4;
        u64 reference4kBytes = 0;
        bool unlocksTwoExtraMipLevels = true;
    };

    struct NtcFeatureSupport
    {
        bool sdkAvailable = false;
        bool vulkanBackendReady = false;
        bool shaderCompilerReady = false;
        bool cooperativeMatrix = false;
        bool subgroupShuffle = false;
        bool temporalReconstructionReady = false;
        bool allowsInferenceOnLoad = true;
        bool allowsInferenceOnSample = false;
    };

    struct NtcMaterialPolicy
    {
        NtcRuntimeMode requestedMode = NtcRuntimeMode::InferenceOnSample;
        NtcCompressionProfile profile = NtcCompressionProfile::NTC_0_2;
        u32 maxMaterialChannels = 16;
        bool allowFallbackToBCn = true;
        bool keepOpacitySeparate = true;
        bool keepHeightSeparateForTessellation = false;
        bool requireTemporalFilteringForSampleMode = true;
    };

    struct NtcAssetDesc
    {
        std::string materialName;
        TextureSetDesc textureSet{};
        NtcCompressionProfile profile = NtcCompressionProfile::NTC_0_2;
        bool storesFeaturePyramid = true;
        bool storesNetworkWeights = true;
        bool storesMipChainJointly = true;
        bool decodesAllChannels = true;
    };

    struct NtcMemoryEstimate
    {
        u64 rawBytes = 0;
        u64 bcnBytes = 0;
        u64 ntcBytes = 0;
        double savingVsRawPercent = 0.0;
        double savingVsBCnPercent = 0.0;
    };

    struct NtcIntegrationPlan
    {
        bool ok = true;
        NtcRuntimeMode selectedMode = NtcRuntimeMode::DisabledFallback;
        NtcCompressionProfile profile = NtcCompressionProfile::NTC_0_2;
        NtcMemoryEstimate memory{};
        std::vector<NtcIntegrationStage> stages;
        std::vector<std::string> warnings;
        std::vector<std::string> errors;
    };

    struct NtcProbeResult
    {
        bool ok = false;
        std::string summary;
        TextureSetProbeResult textureProbe{};
        NtcIntegrationPlan currentPlan{};
        NtcIntegrationPlan futureVulkanPlan{};
    };

    const char* ToString(NtcCompressionProfile profile);
    const char* ToString(NtcRuntimeMode mode);
    const char* ToString(NtcIntegrationStage stage);

    NtcProfileDesc GetNtcProfileDesc(NtcCompressionProfile profile);
    NtcFeatureSupport MakeCurrentFoundationNtcSupport();
    NtcFeatureSupport MakeFutureVulkanNtcSupport();
    NtcMaterialPolicy MakeDefaultNtcMaterialPolicy();

    u64 EstimateNtcTextureSetBytes(const TextureSetDesc& textureSet, NtcCompressionProfile profile);
    NtcMemoryEstimate EstimateNtcMemory(const TextureSetDesc& textureSet, NtcCompressionProfile profile);
    NtcIntegrationPlan BuildNtcIntegrationPlan(const TextureSetDesc& textureSet, const NtcFeatureSupport& support, const NtcMaterialPolicy& policy = {});

    bool IsNtcGoodCandidate(const TextureSetDesc& textureSet);
    std::string ToDebugString(const NtcMemoryEstimate& estimate);
    std::string ToDebugString(const NtcIntegrationPlan& plan);
    std::string BuildNtcProbeSummary();
    NtcProbeResult BuildNtcProbe();
}
