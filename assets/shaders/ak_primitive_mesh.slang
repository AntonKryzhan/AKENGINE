// AK Engine primitive mesh shader contract.
// Source language: Slang/HLSL, target: SPIR-V for Vulkan.
// Vertex layout matches AK::PackedRenderVertex32:
//   location 0: POSITION float3
//   location 1: NORMAL_OCT snorm16x2 packed in uint
//   location 2: TEXCOORD0 unorm16x2 packed in uint
//   location 3: COLOR0 uint rgba8
//   location 4: MATERIAL_FLAGS uint

struct CameraUniform
{
    float4x4 viewProjection;
    float4 cameraWorldPosition;
};

struct ObjectRecord
{
    float4x4 localToWorld;
    uint materialIndex;
    uint flags;
    uint pad0;
    uint pad1;
};

struct MaterialRecord
{
    float4 baseColor;
    float roughness;
    float metallic;
    float normalScale;
    float pad0;
};

[[vk::binding(0, 0)]] ConstantBuffer<CameraUniform> gCamera;
[[vk::binding(1, 0)]] StructuredBuffer<ObjectRecord> gObjects;
[[vk::binding(2, 0)]] StructuredBuffer<MaterialRecord> gMaterials;

struct PushConstants
{
    uint objectIndex;
    uint drawIndex;
    uint materialOverride;
    uint flags;
};

[[vk::push_constant]] PushConstants gPush;

struct VSInput
{
    float3 position : POSITION;
    uint normalOct : NORMAL0;
    uint uvPacked : TEXCOORD0;
    uint colorRgba8 : COLOR0;
    uint materialAndFlags : TEXCOORD1;
};

struct VSOutput
{
    float4 position : SV_Position;
    float3 worldNormal : NORMAL0;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
    uint materialIndex : TEXCOORD1;
};

float2 DecodeUnorm16x2(uint packedValue)
{
    uint x = packedValue & 0xFFFFu;
    uint y = (packedValue >> 16u) & 0xFFFFu;
    return float2(x, y) / 65535.0;
}

float2 DecodeSnorm16x2(uint packedValue)
{
    int x = int(packedValue & 0xFFFFu);
    int y = int((packedValue >> 16u) & 0xFFFFu);
    if (x >= 32768) x -= 65536;
    if (y >= 32768) y -= 65536;
    return clamp(float2(x, y) / 32767.0, -1.0, 1.0);
}

float3 DecodeOctNormal(uint packedValue)
{
    float2 e = DecodeSnorm16x2(packedValue);
    float3 v = float3(e.x, e.y, 1.0 - abs(e.x) - abs(e.y));
    if (v.z < 0.0)
    {
        float2 old = v.xy;
        v.x = (1.0 - abs(old.y)) * (old.x >= 0.0 ? 1.0 : -1.0);
        v.y = (1.0 - abs(old.x)) * (old.y >= 0.0 ? 1.0 : -1.0);
    }
    return normalize(v);
}

float4 DecodeRgba8(uint packedValue)
{
    float r = float((packedValue >> 0u) & 0xFFu) / 255.0;
    float g = float((packedValue >> 8u) & 0xFFu) / 255.0;
    float b = float((packedValue >> 16u) & 0xFFu) / 255.0;
    float a = float((packedValue >> 24u) & 0xFFu) / 255.0;
    return float4(r, g, b, a);
}

[shader("vertex")]
VSOutput vs_main(VSInput input)
{
    ObjectRecord object = gObjects[gPush.objectIndex];
    float4 world = mul(object.localToWorld, float4(input.position, 1.0));

    VSOutput output;
    output.position = mul(gCamera.viewProjection, world);
    output.worldNormal = normalize(mul((float3x3)object.localToWorld, DecodeOctNormal(input.normalOct)));
    output.uv = DecodeUnorm16x2(input.uvPacked);
    output.color = DecodeRgba8(input.colorRgba8);
    output.materialIndex = (input.materialAndFlags & 0xFFFFu);
    return output;
}

[shader("fragment")]
float4 fs_main(VSOutput input) : SV_Target0
{
    MaterialRecord material = gMaterials[input.materialIndex];
    float3 n = normalize(input.worldNormal);
    float3 lightDir = normalize(float3(0.35, 0.85, 0.25));
    float ndotl = saturate(dot(n, lightDir));
    float3 lit = material.baseColor.rgb * input.color.rgb * (0.08 + ndotl * 0.92);
    return float4(lit, material.baseColor.a * input.color.a);
}
