#pragma once

#include <AK/Core/Types.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace AK
{
    enum class TextureChannelSemantic : u32
    {
        BaseColor,
        Normal,
        Roughness,
        Metalness,
        AmbientOcclusion,
        Height,
        Opacity,
        Emissive,
        Subsurface,
        Custom
    };

    enum class TextureSampleEncoding : u32
    {
        Linear,
        SRGB,
        SignedNormal,
        Scalar
    };

    enum class TextureSetKind : u32
    {
        Generic,
        PBRMaterial,
        TerrainLayer,
        PlanetSurfaceLayer,
        CharacterMaterial,
        UI
    };

    enum class TextureRuntimeCompressionFamily : u32
    {
        Raw,
        BCn,
        ASTC,
        BasisUniversal,
        NeuralTextureCompression
    };

    struct TextureChannelDesc
    {
        TextureChannelSemantic semantic = TextureChannelSemantic::Custom;
        TextureSampleEncoding encoding = TextureSampleEncoding::Linear;
        std::string name;
        std::filesystem::path sourcePath;
        u32 width = 0;
        u32 height = 0;
        u32 channelCount = 1;
        u32 bytesPerChannel = 1;
        float importance = 1.0f;
        bool required = true;
    };

    struct TextureSetDesc
    {
        TextureSetKind kind = TextureSetKind::Generic;
        std::string name;
        std::vector<TextureChannelDesc> channels;
        bool generateMipChain = true;
        bool allowResolutionNormalization = true;
        bool allowChannelRepacking = true;
        TextureRuntimeCompressionFamily preferredCompression = TextureRuntimeCompressionFamily::BCn;
    };

    struct TextureSetValidationReport
    {
        bool ok = true;
        bool sameResolution = true;
        bool hasBaseColor = false;
        bool hasNormal = false;
        bool hasMaterialProperties = false;
        u32 referenceWidth = 0;
        u32 referenceHeight = 0;
        u32 totalOutputChannels = 0;
        u64 uncompressedBytes = 0;
        u64 bcnEstimateBytes = 0;
        std::vector<std::string> warnings;
        std::vector<std::string> errors;
    };

    struct TextureSetProbeResult
    {
        bool ok = false;
        std::string summary;
        TextureSetDesc textureSet{};
        TextureSetValidationReport validation{};
    };

    const char* ToString(TextureChannelSemantic semantic);
    const char* ToString(TextureSampleEncoding encoding);
    const char* ToString(TextureSetKind kind);
    const char* ToString(TextureRuntimeCompressionFamily family);

    TextureChannelDesc MakeTextureChannel(TextureChannelSemantic semantic, std::string name, u32 width, u32 height, u32 channelCount, TextureSampleEncoding encoding = TextureSampleEncoding::Linear, float importance = 1.0f);
    TextureSetDesc BuildDefaultPbrTextureSetDesc(u32 width = 4096, u32 height = 4096);
    TextureSetDesc BuildPlanetSurfaceTextureSetDesc(u32 width = 4096, u32 height = 4096);

    u32 CountOutputChannels(const TextureSetDesc& textureSet);
    u64 EstimateMipMappedTexelCount(u32 width, u32 height, bool includeMipChain);
    u64 EstimateUncompressedTextureSetBytes(const TextureSetDesc& textureSet);
    u64 EstimateBCnTextureSetBytes(const TextureSetDesc& textureSet, double bitsPerPixelPerChannel = 4.0);

    TextureSetValidationReport ValidateTextureSet(const TextureSetDesc& textureSet);

    std::string ToDebugString(const TextureSetValidationReport& report);
    std::string ToDebugString(const TextureChannelDesc& channel);
    std::string BuildTextureSetProbeSummary();
    TextureSetProbeResult BuildTextureSetProbe();
}
