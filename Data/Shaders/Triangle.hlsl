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
    [[vk::location(3)]] uint InstanceID : SV_InstanceID;
};

struct ObjectData {
    uint textureIndex;
    uint normalIndex;
    uint metallicRoughnessIndex;
    uint occlusionIndex;
    uint emissiveIndex;
    uint samplerIndex;
    uint isVisible;
    uint _pad0;

    float4 albedoFactor;
    float metallicFactor;
    float roughnessFactor;
    float2 padding;

    float4x4 mvp;
    float4x4 worldMatrix;
    float4 boundingSphere;
    float4 highlightColor;
};

struct PushParams {
    uint frameOffset;
    uint useCustomVP;
    float2 pad;
    float4x4 customViewProj;
};

[[vk::push_constant]]
ConstantBuffer<PushParams> pushParams;

StructuredBuffer<ObjectData> objects : register(t2, space0);

[[vk::binding(0, 0)]]
SamplerState samplers[];

[[vk::binding(1, 0)]]
Texture2D textures[];

PSInput VSMain(VSInput input, uint instanceID : SV_InstanceID) {
    PSInput output;
    uint objIndex = pushParams.frameOffset + instanceID;

    if (pushParams.useCustomVP != 0) {
        output.Pos = mul(pushParams.customViewProj, mul(objects[objIndex].worldMatrix, float4(input.Pos, 1.0)));
    } else {
        output.Pos = mul(pushParams.customViewProj, mul(objects[objIndex].worldMatrix, float4(input.Pos, 1.0)));
    }

    output.WorldPos = input.Pos;
    output.Normal = normalize(mul((float3x3)objects[objIndex].worldMatrix, input.Normal));
    output.UV = input.UV;
    output.InstanceID = objIndex;
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET {
    ObjectData obj = objects[input.InstanceID];

    float4 albedo = textures[obj.textureIndex].Sample(samplers[obj.samplerIndex], input.UV) * obj.albedoFactor;

    float3 lightDir = normalize(float3(0.5, 1.0, 0.5));
    float3 viewDir = normalize(float3(0.0, 0.0, 1.0));
    float3 halfDir = normalize(lightDir + viewDir);

    float3 N = normalize(input.Normal);
    float diff = max(dot(N, lightDir), 0.0);

    float specPower = exp2(10.0 * (1.0 - obj.roughnessFactor) + 1.0);
    float specIntensity = (1.0 - obj.roughnessFactor) * obj.metallicFactor;

    float3 spec = albedo.rgb * pow(max(dot(N, halfDir), 0.0), specPower) * specIntensity;

    float ambient = 0.4;
    float3 finalColor = albedo.rgb * (diff + ambient) + spec;

    if (obj.highlightColor.a > 0.0) {
        finalColor = lerp(finalColor, obj.highlightColor.rgb, obj.highlightColor.a);
    }

    return float4(finalColor, albedo.a);
}
