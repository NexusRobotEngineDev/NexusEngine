struct VSInput {
    uint VertexIndex : SV_VertexID;
};

struct PSInput {
    float4 Pos : SV_POSITION;
    float4 Color : COLOR0;
};

PSInput VSMain(VSInput input) {
    PSInput output;
    float2 positions[3] = {
        float2(0.0, -0.5),
        float2(0.5, 0.5),
        float2(-0.5, 0.5)
    };
    float4 colors[3] = {
        float4(1.0, 0.0, 0.0, 1.0),
        float4(0.0, 1.0, 0.0, 1.0),
        float4(0.0, 0.0, 1.0, 1.0)
    };

    output.Pos = float4(positions[input.VertexIndex], 0.0, 1.0);
    output.Color = colors[input.VertexIndex];
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET {
    return input.Color;
}
