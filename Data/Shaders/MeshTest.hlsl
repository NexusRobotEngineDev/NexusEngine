struct MeshConstants {
    float4 color;
};

[[vk::push_constant]]
MeshConstants constants;

struct Payload {
    uint meshletIndex;
};

struct VSOutput {
    float4 position : SV_POSITION;
    float4 color    : COLOR;
};

[numthreads(1, 1, 1)]
void ASMain(uint gtid : SV_GroupThreadID) {
    Payload p;
    p.meshletIndex = 0;
    DispatchMesh(1, 1, 1, p);
}

[numthreads(3, 1, 1)]
[outputtopology("triangle")]
void MSMain(
    uint gtid : SV_GroupThreadID,
    in payload Payload p,
    out indices uint3 tris[1],
    out vertices VSOutput verts[3]
) {
    SetMeshOutputs(3, 1);

    if (gtid < 3) {
        float2 positions[3] = {
            float2(0.0, -0.5),
            float2(0.5, 0.5),
            float2(-0.5, 0.5)
        };
        verts[gtid].position = float4(positions[gtid], 0.0, 1.0);
        verts[gtid].color = constants.color;
    }

    if (gtid == 0) {
        tris[0] = uint3(0, 1, 2);
    }
}

float4 PSMain(VSOutput input) : SV_Target {
    return input.color;
}
