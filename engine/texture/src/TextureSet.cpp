#include <AK/Texture/TextureSet.hpp>

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <utility>

namespace AK
{
    namespace
    {
        u64 SafeMul(u64 a, u64 b)
        {
            if (a == 0u || b == 0u)
            {
                return 0u;
            }
            constexpr u64 MaxU64 = ~0ull;
            if (a > MaxU64 / b)
            {
                return MaxU64;
            }
            return a * b;
        }

        u64 SafeAdd(u64 a, u64 b)
        {
            constexpr u64 MaxU64 = ~0ull;
            if (a > MaxU64 - b)
            {
                return MaxU64;
            }
            return a + b;
        }

        std::string BytesToText(u64 bytes)
        {
            std::ostringstream out;
            out << std::fixed << std::setprecision(2) << (static_cast<double>(bytes) / (1024.0 * 1024.0)) << " MB";
            return out.str();
        }
    }

    const char* ToString(TextureChannelSemantic semantic)
    {
        switch (semantic)
        {
        case TextureChannelSemantic::BaseColor:
            return "BaseColor";
        case TextureChannelSemantic::Normal:
            return "Normal";
        case TextureChannelSemantic::Roughness:
            return "Roughness";
        case TextureChannelSemantic::Metalness:
            return "Metalness";
        case TextureChannelSemantic::AmbientOcclusion:
            return "AmbientOcclusion";
        case TextureChannelSemantic::Height:
            return "Height";
        case TextureChannelSemantic::Opacity:
            return "Opacity";
        case TextureChannelSemantic::Emissive:
            return "Emissive";
        case TextureChannelSemantic::Subsurface:
            return "Subsurface";
        case TextureChannelSemantic::Custom:
            return "Custom";
        default:
            return "Unknown";
        }
    }

    const char* ToString(TextureSampleEncoding encoding)
    {
        switch (encoding)
        {
        case TextureSampleEncoding::Linear:
            return "Linear";
        case TextureSampleEncoding::SRGB:
            return "SRGB";
        case TextureSampleEncoding::SignedNormal:
            return "SignedNormal";
        case TextureSampleEncoding::Scalar:
            return "Scalar";
        default:
            return "Unknown";
        }
    }

    const char* ToString(TextureSetKind kind)
    {
        switch (kind)
        {
        case TextureSetKind::Generic:
            return "Generic";
        case TextureSetKind::PBRMaterial:
            return "PBRMaterial";
        case TextureSetKind::TerrainLayer:
            return "TerrainLayer";
        case TextureSetKind::PlanetSurfaceLayer:
            return "PlanetSurfaceLayer";
        case TextureSetKind::CharacterMaterial:
            return "CharacterMaterial";
        case TextureSetKind::UI:
            return "UI";
        default:
            return "Unknown";
        }
    }

    const char* ToString(TextureRuntimeCompressionFamily family)
    {
        switch (family)
        {
        case TextureRuntimeCompressionFamily::Raw:
            return "Raw";
        case TextureRuntimeCompressionFamily::BCn:
            return "BCn";
        case TextureRuntimeCompressionFamily::ASTC:
            return "ASTC";
        case TextureRuntimeCompressionFamily::BasisUniversal:
            return "BasisUniversal";
        case TextureRuntimeCompressionFamily::NeuralTextureCompression:
            return "NeuralTextureCompression";
        default:
            return "Unknown";
        }
    }

    TextureChannelDesc MakeTextureChannel(TextureChannelSemantic semantic, std::string name, u32 width, u32 height, u32 channelCount, TextureSampleEncoding encoding, float importance)
    {
        TextureChannelDesc channel{};
        channel.semantic = semantic;
        channel.encoding = encoding;
        channel.name = std::move(name);
        channel.width = width;
        channel.height = height;
        channel.channelCount = std::max(1u, channelCount);
        channel.bytesPerChannel = 1u;
        channel.importance = std::max(0.0f, importance);
        channel.required = true;
        return channel;
    }

    TextureSetDesc BuildDefaultPbrTextureSetDesc(u32 width, u32 height)
    {
        TextureSetDesc textureSet{};
        textureSet.kind = TextureSetKind::PBRMaterial;
        textureSet.name = "DefaultPBR_9ch";
        textureSet.preferredCompression = TextureRuntimeCompressionFamily::NeuralTextureCompression;
        textureSet.channels.push_back(MakeTextureChannel(TextureChannelSemantic::BaseColor, "albedo", width, height, 3u, TextureSampleEncoding::SRGB, 1.0f));
        textureSet.channels.push_back(MakeTextureChannel(TextureChannelSemantic::Normal, "normal", width, height, 3u, TextureSampleEncoding::SignedNormal, 1.2f));
        textureSet.channels.push_back(MakeTextureChannel(TextureChannelSemantic::Roughness, "roughness", width, height, 1u, TextureSampleEncoding::Scalar, 1.0f));
        textureSet.channels.push_back(MakeTextureChannel(TextureChannelSemantic::Metalness, "metalness", width, height, 1u, TextureSampleEncoding::Scalar, 0.8f));
        textureSet.channels.push_back(MakeTextureChannel(TextureChannelSemantic::AmbientOcclusion, "ao", width, height, 1u, TextureSampleEncoding::Scalar, 0.8f));
        return textureSet;
    }

    TextureSetDesc BuildPlanetSurfaceTextureSetDesc(u32 width, u32 height)
    {
        TextureSetDesc textureSet = BuildDefaultPbrTextureSetDesc(width, height);
        textureSet.kind = TextureSetKind::PlanetSurfaceLayer;
        textureSet.name = "PlanetSurface_10ch";
        textureSet.channels.push_back(MakeTextureChannel(TextureChannelSemantic::Height, "height", width, height, 1u, TextureSampleEncoding::Scalar, 1.0f));
        return textureSet;
    }

    u32 CountOutputChannels(const TextureSetDesc& textureSet)
    {
        u32 count = 0;
        for (const TextureChannelDesc& channel : textureSet.channels)
        {
            count += std::max(1u, channel.channelCount);
        }
        return count;
    }

    u64 EstimateMipMappedTexelCount(u32 width, u32 height, bool includeMipChain)
    {
        u64 total = 0u;
        u32 mipWidth = std::max(1u, width);
        u32 mipHeight = std::max(1u, height);
        for (;;)
        {
            total = SafeAdd(total, SafeMul(static_cast<u64>(mipWidth), static_cast<u64>(mipHeight)));
            if (!includeMipChain || (mipWidth == 1u && mipHeight == 1u))
            {
                break;
            }
            mipWidth = std::max(1u, mipWidth / 2u);
            mipHeight = std::max(1u, mipHeight / 2u);
        }
        return total;
    }

    u64 EstimateUncompressedTextureSetBytes(const TextureSetDesc& textureSet)
    {
        u64 total = 0u;
        for (const TextureChannelDesc& channel : textureSet.channels)
        {
            const u64 texels = EstimateMipMappedTexelCount(channel.width, channel.height, textureSet.generateMipChain);
            const u64 channelBytes = SafeMul(SafeMul(texels, static_cast<u64>(std::max(1u, channel.channelCount))), static_cast<u64>(std::max(1u, channel.bytesPerChannel)));
            total = SafeAdd(total, channelBytes);
        }
        return total;
    }

    u64 EstimateBCnTextureSetBytes(const TextureSetDesc& textureSet, double bitsPerPixelPerChannel)
    {
        if (bitsPerPixelPerChannel <= 0.0)
        {
            return 0u;
        }

        u64 total = 0u;
        for (const TextureChannelDesc& channel : textureSet.channels)
        {
            const double texels = static_cast<double>(EstimateMipMappedTexelCount(channel.width, channel.height, textureSet.generateMipChain));
            const double bits = texels * static_cast<double>(std::max(1u, channel.channelCount)) * bitsPerPixelPerChannel;
            total = SafeAdd(total, static_cast<u64>(bits / 8.0 + 0.5));
        }
        return total;
    }

    TextureSetValidationReport ValidateTextureSet(const TextureSetDesc& textureSet)
    {
        TextureSetValidationReport report{};
        report.totalOutputChannels = CountOutputChannels(textureSet);
        report.uncompressedBytes = EstimateUncompressedTextureSetBytes(textureSet);
        report.bcnEstimateBytes = EstimateBCnTextureSetBytes(textureSet, 4.0);

        const auto error = [&report](const std::string& message)
        {
            report.ok = false;
            report.errors.push_back(message);
        };
        const auto warning = [&report](const std::string& message)
        {
            report.warnings.push_back(message);
        };

        if (textureSet.channels.empty())
        {
            error("texture set has no channels");
            return report;
        }

        report.referenceWidth = textureSet.channels.front().width;
        report.referenceHeight = textureSet.channels.front().height;

        if (report.referenceWidth == 0u || report.referenceHeight == 0u)
        {
            error("reference resolution must be non-zero");
        }

        for (const TextureChannelDesc& channel : textureSet.channels)
        {
            if (channel.width == 0u || channel.height == 0u)
            {
                error("channel " + channel.name + " has zero resolution");
            }
            if (channel.width != report.referenceWidth || channel.height != report.referenceHeight)
            {
                report.sameResolution = false;
            }
            if (channel.channelCount == 0u || channel.channelCount > 4u)
            {
                error("channel " + channel.name + " must contain 1..4 scalar channels");
            }
            if (channel.bytesPerChannel == 0u || channel.bytesPerChannel > 4u)
            {
                error("channel " + channel.name + " bytesPerChannel must be 1..4");
            }
            if (channel.importance <= 0.0f)
            {
                warning("channel " + channel.name + " has zero importance");
            }
            if (channel.semantic == TextureChannelSemantic::BaseColor)
            {
                report.hasBaseColor = true;
            }
            if (channel.semantic == TextureChannelSemantic::Normal)
            {
                report.hasNormal = true;
            }
            if (channel.semantic == TextureChannelSemantic::Roughness || channel.semantic == TextureChannelSemantic::Metalness || channel.semantic == TextureChannelSemantic::AmbientOcclusion || channel.semantic == TextureChannelSemantic::Height)
            {
                report.hasMaterialProperties = true;
            }
        }

        if (!report.sameResolution && !textureSet.allowResolutionNormalization)
        {
            error("texture set channels do not share one resolution and normalization is disabled");
        }
        else if (!report.sameResolution)
        {
            warning("texture set channels must be normalized to one resolution before NTC compression");
        }

        if (textureSet.kind == TextureSetKind::PBRMaterial || textureSet.kind == TextureSetKind::PlanetSurfaceLayer || textureSet.kind == TextureSetKind::TerrainLayer)
        {
            if (!report.hasBaseColor)
            {
                warning("material texture set has no base-color channel");
            }
            if (!report.hasNormal)
            {
                warning("material texture set has no normal channel");
            }
            if (!report.hasMaterialProperties)
            {
                warning("material texture set has no roughness/metalness/AO/height channels");
            }
        }

        if (report.totalOutputChannels < 4u && textureSet.preferredCompression == TextureRuntimeCompressionFamily::NeuralTextureCompression)
        {
            warning("NTC is usually most valuable for multi-channel material texture sets");
        }

        return report;
    }

    std::string ToDebugString(const TextureSetValidationReport& report)
    {
        std::ostringstream out;
        out << "textureSet ok=" << (report.ok ? "yes" : "no")
            << " resolution=" << report.referenceWidth << "x" << report.referenceHeight
            << " channels=" << report.totalOutputChannels
            << " sameRes=" << (report.sameResolution ? "yes" : "no")
            << " raw=" << BytesToText(report.uncompressedBytes)
            << " bcn~=" << BytesToText(report.bcnEstimateBytes)
            << " warnings=" << report.warnings.size()
            << " errors=" << report.errors.size();
        return out.str();
    }

    std::string ToDebugString(const TextureChannelDesc& channel)
    {
        std::ostringstream out;
        out << channel.name << " " << ToString(channel.semantic)
            << " " << channel.width << "x" << channel.height
            << " ch=" << channel.channelCount
            << " enc=" << ToString(channel.encoding)
            << " importance=" << std::fixed << std::setprecision(2) << channel.importance;
        return out.str();
    }

    std::string BuildTextureSetProbeSummary()
    {
        const TextureSetProbeResult probe = BuildTextureSetProbe();
        return probe.summary;
    }

    TextureSetProbeResult BuildTextureSetProbe()
    {
        TextureSetProbeResult probe{};
        probe.textureSet = BuildDefaultPbrTextureSetDesc(4096u, 4096u);
        probe.validation = ValidateTextureSet(probe.textureSet);
        probe.ok = probe.validation.ok && probe.validation.totalOutputChannels == 9u && probe.validation.sameResolution;

        std::ostringstream out;
        out << "TextureSet probe: " << (probe.ok ? "ok" : "failed")
            << " kind=" << ToString(probe.textureSet.kind)
            << " channels=" << probe.validation.totalOutputChannels
            << " raw=" << BytesToText(probe.validation.uncompressedBytes)
            << " bcn~=" << BytesToText(probe.validation.bcnEstimateBytes);
        probe.summary = out.str();
        return probe;
    }
}
