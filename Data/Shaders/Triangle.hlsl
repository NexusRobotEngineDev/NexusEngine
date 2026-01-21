struct VSInput {
    float3 Pos : POSITION;
    float2 UV : TEXCOORD0;
};

struct PSInput {
    float4 Pos : SV_POSITION;
    float4 Color : COLOR0;
    float2 UV : TEXCOORD0;
};

struct BindlessConstants {
    uint textureIndex;
    uint samplerIndex;
    uint padding0;
    uint padding1;
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
    output.Pos = mul(constants.mvp, float4(input.Pos, 1.0));
    output.Color = float4(1.0, 1.0, 1.0, 1.0);
    output.UV = input.UV;
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET {
    return textures[constants.textureIndex].Sample(samplers[constants.samplerIndex], input.UV);
}
