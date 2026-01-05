struct VSInput {
    uint VertexIndex : SV_VertexID;
};

struct PSInput {
    float4 Pos : SV_POSITION;
    float4 Color : COLOR0;
    float2 UV : TEXCOORD0;
};

struct BindlessConstants {
    uint textureIndex;
    uint samplerIndex;
};

[[vk::push_constant]]
BindlessConstants constants;

[[vk::set(0)]] [[vk::binding(0)]]
SamplerState samplers[];

[[vk::set(0)]] [[vk::binding(1)]]
Texture2D textures[];

PSInput VSMain(VSInput input) {
    PSInput output;
    float2 positions[3] = {
        float2(0.0, -0.5),
        float2(0.5, 0.5),
        float2(-0.5, 0.5)
    };
    float2 uvs[3] = {
        float2(0.5, 0.0),
        float2(1.0, 1.0),
        float2(0.0, 1.0)
    };

    output.Pos = float4(positions[input.VertexIndex], 0.0, 1.0);
    output.Color = float4(1.0, 1.0, 1.0, 1.0);
    output.UV = uvs[input.VertexIndex];
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET {
    return textures[constants.textureIndex].Sample(samplers[constants.samplerIndex], input.UV);
}
