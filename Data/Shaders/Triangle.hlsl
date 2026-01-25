struct VSInput {
    [[vk::location(0)]] float3 Pos : POSITION;
    [[vk::location(1)]] float2 UV : TEXCOORD0;
    [[vk::location(2)]] float3 Normal : NORMAL;
};

struct PSInput {
    float4 Pos : SV_POSITION;
    [[vk::location(0)]] float3 WorldPos : POSITION0;
    [[vk::location(1)]] float3 Normal : NORMAL0;
    [[vk::location(2)]] float2 UV : TEXCOORD0;
};

struct BindlessConstants {
    uint textureIndex;
    uint normalIndex;
    uint metallicRoughnessIndex;
    uint occlusionIndex;
    uint emissiveIndex;
    uint samplerIndex;

    float4 albedoFactor;
    float metallicFactor;
    float roughnessFactor;
    float2 padding;

    float4x4 mvp;
};

[[vk::push_constant]]
ConstantBuffer<BindlessConstants> constants;

[[vk::binding(0, 0)]]
SamplerState samplers[];

[[vk::binding(1, 0)]]
Texture2D textures[];

PSInput VSMain(VSInput input) {
    PSInput output;
    output.Pos = float4(input.Pos, 1.0) * constants.mvp;
    output.WorldPos = input.Pos;
    output.Normal = input.Normal;
    output.UV = input.UV;
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET {
    float4 albedo = textures[constants.textureIndex].Sample(samplers[constants.samplerIndex], input.UV) * constants.albedoFactor;

    float3 lightDir = normalize(float3(1.0, 1.0, 1.0));
    float3 viewDir = normalize(float3(0.0, 0.0, 1.0));
    float3 halfDir = normalize(lightDir + viewDir);

    float3 N = normalize(input.Normal);
    float diff = max(dot(N, lightDir), 0.0);

    float spec = pow(max(dot(N, halfDir), 0.0), 32.0) * constants.metallicFactor;

    float ambient = 0.2;
    float3 finalColor = albedo.rgb * (diff + ambient) + spec;

    return float4(finalColor, albedo.a);
}
