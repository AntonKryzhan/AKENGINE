#include <AK/NTC/NeuralTextureCompression.hpp>

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace AK
{
    namespace
    {
        constexpr double Reference4kPixels = 4096.0 * 4096.0;

        u64 MegabytesToBytes(double megabytes)
        {
            return static_cast<u64>(megabytes * 1024.0 * 1024.0 + 0.5);
        }

        std::string BytesToText(u64 bytes)
        {
            std::ostringstream out;
            out << std::fixed << std::setprecision(2) << (static_cast<double>(bytes) / (1024.0 * 1024.0)) << " MB";
            return out.str();
        }

        double SavingPercent(u64 baselineBytes, u64 compressedBytes)
        {
            if (baselineBytes == 0u || compressedBytes >= baselineBytes)
            {
                return 0.0;
            }
            return (1.0 - static_cast<double>(compressedBytes) / static_cast<double>(baselineBytes)) * 100.0;
        }

        bool HasSemantic(const TextureSetDesc& textureSet, TextureChannelSemantic semantic)
        {
            for (const TextureChannelDesc& channel : textureSet.channels)
            {
                if (channel.semantic == semantic)
                {
                    return true;
                }
            }
            return false;
        }

        void AddStageList(std::vector<NtcIntegrationStage>& stages)
        {
            stages.push_back(NtcIntegrationStage::Foundation);
            stages.push_back(NtcIntegrationStage::CookerSdkBridge);
            stages.push_back(NtcIntegrationStage::VulkanFeatureDetection);
            stages.push_back(NtcIntegrationStage::ShaderDecodePath);
            stages.push_back(NtcIntegrationStage::TemporalFiltering);
            stages.push_back(NtcIntegrationStage::MaterialPolicy);
        }
    }

    const char* ToString(NtcCompressionProfile profile)
    {
        switch (profile)
        {
        case NtcCompressionProfile::NTC_0_2:
            return "NTC 0.2";
        case NtcCompressionProfile::NTC_0_5:
            return "NTC 0.5";
        case NtcCompressionProfile::NTC_1_0:
            return "NTC 1.0";
        case NtcCompressionProfile::NTC_2_25:
            return "NTC 2.25";
        case NtcCompressionProfile::Custom:
            return "Custom";
        default:
            return "Unknown";
        }
    }

    const char* ToString(NtcRuntimeMode mode)
    {
        switch (mode)
        {
        case NtcRuntimeMode::DisabledFallback:
            return "DisabledFallback";
        case NtcRuntimeMode::InferenceOnLoadToBCn:
            return "InferenceOnLoadToBCn";
        case NtcRuntimeMode::InferenceOnSample:
            return "InferenceOnSample";
        case NtcRuntimeMode::Hybrid:
            return "Hybrid";
        default:
            return "Unknown";
        }
    }

    const char* ToString(NtcIntegrationStage stage)
    {
        switch (stage)
        {
        case NtcIntegrationStage::Foundation:
            return "Foundation";
        case NtcIntegrationStage::CookerSdkBridge:
            return "CookerSdkBridge";
        case NtcIntegrationStage::VulkanFeatureDetection:
            return "VulkanFeatureDetection";
        case NtcIntegrationStage::ShaderDecodePath:
            return "ShaderDecodePath";
        case NtcIntegrationStage::TemporalFiltering:
            return "TemporalFiltering";
        case NtcIntegrationStage::MaterialPolicy:
            return "MaterialPolicy";
        default:
            return "Unknown";
        }
    }

    NtcProfileDesc GetNtcProfileDesc(NtcCompressionProfile profile)
    {
        NtcProfileDesc desc{};
        desc.profile = profile;
        switch (profile)
        {
        case NtcCompressionProfile::NTC_0_2:
            desc.bitsPerPixelPerChannel = 0.2;
            desc.rootG0Resolution = 1024u;
            desc.g0Channels = 8u;
            desc.g0Bits = 2u;
            desc.g1Channels = 12u;
            desc.g1Bits = 4u;
            desc.reference4kBytes = MegabytesToBytes(3.524);
            desc.unlocksTwoExtraMipLevels = true;
            break;
        case NtcCompressionProfile::NTC_0_5:
            desc.bitsPerPixelPerChannel = 0.5;
            desc.rootG0Resolution = 1024u;
            desc.g0Channels = 12u;
            desc.g0Bits = 4u;
            desc.g1Channels = 20u;
            desc.g1Bits = 4u;
            desc.reference4kBytes = MegabytesToBytes(8.527);
            desc.unlocksTwoExtraMipLevels = true;
            break;
        case NtcCompressionProfile::NTC_1_0:
            desc.bitsPerPixelPerChannel = 1.0;
            desc.rootG0Resolution = 2048u;
            desc.g0Channels = 12u;
            desc.g0Bits = 2u;
            desc.g1Channels = 10u;
            desc.g1Bits = 4u;
            desc.reference4kBytes = MegabytesToBytes(17.025);
            desc.unlocksTwoExtraMipLevels = false;
            break;
        case NtcCompressionProfile::NTC_2_25:
            desc.bitsPerPixelPerChannel = 2.25;
            desc.rootG0Resolution = 2048u;
            desc.g0Channels = 16u;
            desc.g0Bits = 4u;
            desc.g1Channels = 12u;
            desc.g1Bits = 4u;
            desc.reference4kBytes = MegabytesToBytes(38.027);
            desc.unlocksTwoExtraMipLevels = false;
            break;
        case NtcCompressionProfile::Custom:
        default:
            desc.bitsPerPixelPerChannel = 1.0;
            desc.rootG0Resolution = 1024u;
            desc.g0Channels = 8u;
            desc.g0Bits = 4u;
            desc.g1Channels = 8u;
            desc.g1Bits = 4u;
            desc.reference4kBytes = MegabytesToBytes(16.0);
            desc.unlocksTwoExtraMipLevels = false;
            break;
        }
        return desc;
    }

    NtcFeatureSupport MakeCurrentFoundationNtcSupport()
    {
        NtcFeatureSupport support{};
        support.sdkAvailable = false;
        support.vulkanBackendReady = false;
        support.shaderCompilerReady = false;
        support.cooperativeMatrix = false;
        support.subgroupShuffle = false;
        support.temporalReconstructionReady = false;
        support.allowsInferenceOnLoad = true;
        support.allowsInferenceOnSample = false;
        return support;
    }

    NtcFeatureSupport MakeFutureVulkanNtcSupport()
    {
        NtcFeatureSupport support{};
        support.sdkAvailable = true;
        support.vulkanBackendReady = true;
        support.shaderCompilerReady = true;
        support.cooperativeMatrix = true;
        support.subgroupShuffle = true;
        support.temporalReconstructionReady = true;
        support.allowsInferenceOnLoad = true;
        support.allowsInferenceOnSample = true;
        return support;
    }

    NtcMaterialPolicy MakeDefaultNtcMaterialPolicy()
    {
        NtcMaterialPolicy policy{};
        policy.requestedMode = NtcRuntimeMode::InferenceOnSample;
        policy.profile = NtcCompressionProfile::NTC_0_2;
        policy.maxMaterialChannels = 16u;
        policy.allowFallbackToBCn = true;
        policy.keepOpacitySeparate = true;
        policy.keepHeightSeparateForTessellation = false;
        policy.requireTemporalFilteringForSampleMode = true;
        return policy;
    }

    u64 EstimateNtcTextureSetBytes(const TextureSetDesc& textureSet, NtcCompressionProfile profile)
    {
        if (textureSet.channels.empty())
        {
            return 0u;
        }

        const TextureSetValidationReport validation = ValidateTextureSet(textureSet);
        const NtcProfileDesc profileDesc = GetNtcProfileDesc(profile);
        const double texelRatio = (static_cast<double>(std::max(1u, validation.referenceWidth)) * static_cast<double>(std::max(1u, validation.referenceHeight))) / Reference4kPixels;
        return static_cast<u64>(static_cast<double>(profileDesc.reference4kBytes) * texelRatio + 0.5);
    }

    NtcMemoryEstimate EstimateNtcMemory(const TextureSetDesc& textureSet, NtcCompressionProfile profile)
    {
        NtcMemoryEstimate estimate{};
        estimate.rawBytes = EstimateUncompressedTextureSetBytes(textureSet);
        estimate.bcnBytes = EstimateBCnTextureSetBytes(textureSet, 4.0);
        estimate.ntcBytes = EstimateNtcTextureSetBytes(textureSet, profile);
        estimate.savingVsRawPercent = SavingPercent(estimate.rawBytes, estimate.ntcBytes);
        estimate.savingVsBCnPercent = SavingPercent(estimate.bcnBytes, estimate.ntcBytes);
        return estimate;
    }

    bool IsNtcGoodCandidate(const TextureSetDesc& textureSet)
    {
        const TextureSetValidationReport validation = ValidateTextureSet(textureSet);
        if (!validation.ok || !validation.sameResolution)
        {
            return false;
        }
        if (validation.totalOutputChannels < 4u)
        {
            return false;
        }
        return HasSemantic(textureSet, TextureChannelSemantic::BaseColor)
            && HasSemantic(textureSet, TextureChannelSemantic::Normal)
            && validation.hasMaterialProperties;
    }

    NtcIntegrationPlan BuildNtcIntegrationPlan(const TextureSetDesc& textureSet, const NtcFeatureSupport& support, const NtcMaterialPolicy& policy)
    {
        NtcIntegrationPlan plan{};
        plan.profile = policy.profile;
        plan.memory = EstimateNtcMemory(textureSet, policy.profile);
        AddStageList(plan.stages);

        const TextureSetValidationReport validation = ValidateTextureSet(textureSet);
        const auto error = [&plan](const std::string& message)
        {
            plan.ok = false;
            plan.errors.push_back(message);
        };
        const auto warning = [&plan](const std::string& message)
        {
            plan.warnings.push_back(message);
        };

        if (!validation.ok)
        {
            error("texture set validation failed");
        }
        if (!validation.sameResolution)
        {
            warning("texture channels need resolution normalization before NTC compression");
        }
        if (validation.totalOutputChannels > policy.maxMaterialChannels)
        {
            warning("texture set exceeds configured NTC material channel budget");
        }
        if (!IsNtcGoodCandidate(textureSet))
        {
            warning("texture set is not an ideal NTC candidate; keep BCn fallback available");
        }
        if (policy.keepOpacitySeparate && HasSemantic(textureSet, TextureChannelSemantic::Opacity))
        {
            warning("opacity should remain available as a separate fallback texture for depth/shadow passes");
        }
        if (policy.keepHeightSeparateForTessellation && HasSemantic(textureSet, TextureChannelSemantic::Height))
        {
            warning("height/displacement may need separate access for tessellation or terrain generation");
        }

        if (policy.requestedMode == NtcRuntimeMode::InferenceOnSample || policy.requestedMode == NtcRuntimeMode::Hybrid)
        {
            if (support.allowsInferenceOnSample && support.vulkanBackendReady && support.shaderCompilerReady && support.cooperativeMatrix && support.subgroupShuffle)
            {
                if (policy.requireTemporalFilteringForSampleMode && !support.temporalReconstructionReady)
                {
                    warning("sample-mode decode should wait for temporal reconstruction/stochastic filtering support");
                    plan.selectedMode = policy.allowFallbackToBCn ? NtcRuntimeMode::InferenceOnLoadToBCn : NtcRuntimeMode::DisabledFallback;
                }
                else
                {
                    plan.selectedMode = policy.requestedMode;
                }
            }
            else
            {
                warning("runtime sample decode requires Vulkan backend, shader compiler, cooperative matrix and subgroup support");
                plan.selectedMode = support.allowsInferenceOnLoad ? NtcRuntimeMode::InferenceOnLoadToBCn : NtcRuntimeMode::DisabledFallback;
            }
        }
        else
        {
            plan.selectedMode = policy.requestedMode;
        }

        if (!support.sdkAvailable)
        {
            warning("RTXNTC SDK bridge is not wired yet; cooker will keep NTC payloads as planned assets only");
        }
        if (plan.selectedMode == NtcRuntimeMode::DisabledFallback && !policy.allowFallbackToBCn)
        {
            error("NTC disabled and fallback is forbidden");
        }

        return plan;
    }

    std::string ToDebugString(const NtcMemoryEstimate& estimate)
    {
        std::ostringstream out;
        out << "raw=" << BytesToText(estimate.rawBytes)
            << " bcn~=" << BytesToText(estimate.bcnBytes)
            << " ntc~=" << BytesToText(estimate.ntcBytes)
            << " saveRaw=" << std::fixed << std::setprecision(1) << estimate.savingVsRawPercent << "%"
            << " saveBCn=" << std::fixed << std::setprecision(1) << estimate.savingVsBCnPercent << "%";
        return out.str();
    }

    std::string ToDebugString(const NtcIntegrationPlan& plan)
    {
        std::ostringstream out;
        out << "ntc plan ok=" << (plan.ok ? "yes" : "no")
            << " mode=" << ToString(plan.selectedMode)
            << " profile=" << ToString(plan.profile)
            << " " << ToDebugString(plan.memory)
            << " warnings=" << plan.warnings.size()
            << " errors=" << plan.errors.size();
        return out.str();
    }

    std::string BuildNtcProbeSummary()
    {
        return BuildNtcProbe().summary;
    }

    NtcProbeResult BuildNtcProbe()
    {
        NtcProbeResult probe{};
        probe.textureProbe = BuildTextureSetProbe();
        NtcMaterialPolicy policy = MakeDefaultNtcMaterialPolicy();
        policy.profile = NtcCompressionProfile::NTC_0_2;
        probe.currentPlan = BuildNtcIntegrationPlan(probe.textureProbe.textureSet, MakeCurrentFoundationNtcSupport(), policy);
        probe.futureVulkanPlan = BuildNtcIntegrationPlan(probe.textureProbe.textureSet, MakeFutureVulkanNtcSupport(), policy);
        probe.ok = probe.textureProbe.ok && probe.currentPlan.ok && probe.futureVulkanPlan.ok && probe.futureVulkanPlan.memory.ntcBytes < probe.futureVulkanPlan.memory.bcnBytes;

        std::ostringstream out;
        out << "NTC probe: " << (probe.ok ? "ok" : "failed")
            << " current=" << ToString(probe.currentPlan.selectedMode)
            << " future=" << ToString(probe.futureVulkanPlan.selectedMode)
            << " profile=" << ToString(policy.profile)
            << " " << ToDebugString(probe.futureVulkanPlan.memory);
        probe.summary = out.str();
        return probe;
    }
}
