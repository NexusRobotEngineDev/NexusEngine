struct VSOutput {
    float4 Pos : SV_POSITION;
    float2 UV : TEXCOORD0;
};

VSOutput VSMain(uint vertexID : SV_VertexID) {
    VSOutput output;
    output.UV = float2((vertexID << 1) & 2, vertexID & 2);
    output.Pos = float4(output.UV * 2.0f - 1.0f, 0.0f, 1.0f);
    output.Pos.y = -output.Pos.y;
    return output;
}

Texture2D<float4> computeResultRT : register(t12, space0);
SamplerState samplerState : register(s13, space0);

float4 PSMain(VSOutput input) : SV_TARGET {
    return computeResultRT.SampleLevel(samplerState, input.UV, 0);
}
