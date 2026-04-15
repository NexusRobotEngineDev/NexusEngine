struct MeshletData {
    uint vertexOffset;
    uint triangleOffset;
    uint vertexCount;
    uint triangleCount;
};

struct MeshletBounds {
    float3 center;
    float radius;
    float3 coneAxis;
    float coneCutoff;
};

struct VSOutput {
    float4 position : SV_POSITION;
    [[vk::location(0)]] float3 worldPos : POSITION0;
    [[vk::location(1)]] float3 normal : NORMAL0;
    [[vk::location(2)]] float2 uv : TEXCOORD0;
    [[vk::location(3)]] float4 color : COLOR0;
};

struct Vertex {
    float3 position;
    float2 uv;
    float3 normal;
};

struct MeshletPushGlobalParams {
    float4x4 viewProj;
    float3 cameraPos;
    float _pad0;
};

[[vk::push_constant]]
ConstantBuffer<MeshletPushGlobalParams> pushParams;

struct MeshletInstanceData {
    uint meshletOffset;
    uint meshletCount;
    uint vertexBufferOffset;
    uint _pad0;
    float4x4 worldMatrix;
    float4 albedoFactor;
};

StructuredBuffer<MeshletData> meshletBuffer : register(t0, space2);
StructuredBuffer<MeshletBounds> boundsBuffer : register(t1, space2);
StructuredBuffer<uint> meshletVertexBuffer : register(t2, space2);
StructuredBuffer<uint> meshletTriangleBuffer : register(t3, space2);
StructuredBuffer<Vertex> globalVertexBuffer : register(t4, space2);
StructuredBuffer<MeshletInstanceData> meshletInstances : register(t5, space2);

struct Payload {
    uint meshletIndices[32];
    uint drawID;
};

groupshared Payload s_Payload;

bool isSphereOutsideFrustum(float3 center, float radius, float4 planes[6]) {
    for (int i = 0; i < 6; i++) {
        if (dot(planes[i].xyz, center) + planes[i].w < -radius) {
            return true;
        }
    }
    return false;
}

bool isBackfacing(float3 coneAxis, float coneCutoff, float3 center, float3 cameraPos) {
    float3 viewDir = normalize(center - cameraPos);
    return dot(viewDir, coneAxis) >= coneCutoff;
}

[numthreads(32, 1, 1)]
void ASMain(uint gtid : SV_GroupThreadID, uint gid : SV_GroupID, [[vk::builtin("DrawIndex")]] uint drawID : DRAWID) {
    MeshletInstanceData instance = meshletInstances[drawID];
    uint meshletIndex = gid * 32 + gtid;
    bool visible = false;

    if (meshletIndex < instance.meshletCount) {
        uint globalIdx = instance.meshletOffset + meshletIndex;
        MeshletBounds b = boundsBuffer[globalIdx];

        float4 worldCenter4 = mul(float4(b.center, 1.0), instance.worldMatrix);
        float3 worldCenter = worldCenter4.xyz;

        float scaleX = length(instance.worldMatrix[0].xyz);
        float scaleY = length(instance.worldMatrix[1].xyz);
        float scaleZ = length(instance.worldMatrix[2].xyz);
        float maxScale = max(scaleX, max(scaleY, scaleZ));
        float worldRadius = b.radius * maxScale;

        float4x4 vp = pushParams.viewProj;
        float4 r0 = vp[0]; float4 r1 = vp[1]; float4 r2 = vp[2]; float4 r3 = vp[3];
        float4 planes[6];
        planes[0] = r3 + r0; planes[1] = r3 - r0;
        planes[2] = r3 + r1; planes[3] = r3 - r1;
        planes[4] = r3 + r2; planes[5] = r3 - r2;
        for (int i = 0; i < 6; i++) {
            float len = length(planes[i].xyz);
            if (len > 0.0001) planes[i] /= len;
        }

        visible = !isSphereOutsideFrustum(worldCenter, worldRadius, planes);

        if (visible && b.coneCutoff < 1.0) {
            float3 worldAxis = normalize(mul(float4(b.coneAxis, 0.0), instance.worldMatrix).xyz);
            visible = !isBackfacing(worldAxis, b.coneCutoff, worldCenter, pushParams.cameraPos);
        }
    }

    if (visible) {
        uint index = WavePrefixCountBits(visible);
        s_Payload.meshletIndices[index] = meshletIndex;
    }

    if (gtid == 0) {
        s_Payload.drawID = drawID;
    }

    uint visibleCount = WaveActiveCountBits(visible);
    DispatchMesh(visibleCount, 1, 1, s_Payload);
}

[numthreads(128, 1, 1)]
[outputtopology("triangle")]
void MSMain(
    uint gtid : SV_GroupThreadID,
    uint gid : SV_GroupID,
    in payload Payload p,
    out indices uint3 tris[124],
    out vertices VSOutput verts[64]
) {
    MeshletInstanceData instance = meshletInstances[p.drawID];
    uint meshletIndex = p.meshletIndices[gid];
    uint globalIdx = instance.meshletOffset + meshletIndex;
    MeshletData m = meshletBuffer[globalIdx];

    SetMeshOutputCounts(m.vertexCount, m.triangleCount);

    if (gtid < m.vertexCount) {
        uint vertexIndex = meshletVertexBuffer[m.vertexOffset + gtid];
        uint globalVertIdx = instance.vertexBufferOffset + vertexIndex;
        Vertex v = globalVertexBuffer[globalVertIdx];

        float4 worldPos = mul(float4(v.position, 1.0), instance.worldMatrix);
        float4 clipPos = mul(worldPos, pushParams.viewProj);

        VSOutput out_v;
        out_v.position = clipPos;
        out_v.worldPos = worldPos.xyz;
        out_v.normal = normalize(mul(float4(v.normal, 0.0), instance.worldMatrix).xyz);
        out_v.uv = v.uv;
        out_v.color = instance.albedoFactor;
        verts[gtid] = out_v;
    }

    if (gtid < m.triangleCount) {
        uint triOffset = m.triangleOffset + gtid * 3;
        uint packedBase = triOffset / 4;
        uint packedMod = triOffset % 4;

        uint packed0 = meshletTriangleBuffer[packedBase];
        uint packed1 = meshletTriangleBuffer[packedBase + 1];

        uint i0, i1, i2;
        if (packedMod == 0) {
            i0 = (packed0 >> 0) & 0xFF;
            i1 = (packed0 >> 8) & 0xFF;
            i2 = (packed0 >> 16) & 0xFF;
        } else if (packedMod == 1) {
            i0 = (packed0 >> 8) & 0xFF;
            i1 = (packed0 >> 16) & 0xFF;
            i2 = (packed0 >> 24) & 0xFF;
        } else if (packedMod == 2) {
            i0 = (packed0 >> 16) & 0xFF;
            i1 = (packed0 >> 24) & 0xFF;
            i2 = (packed1 >> 0) & 0xFF;
        } else {
            i0 = (packed0 >> 24) & 0xFF;
            i1 = (packed1 >> 0) & 0xFF;
            i2 = (packed1 >> 8) & 0xFF;
        }
        tris[gtid] = uint3(i0, i1, i2);
    }
}

[[vk::binding(0, 0)]]
SamplerState samplers[];

[[vk::binding(1, 0)]]
Texture2D textures[];

float4 PSMain(VSOutput input) : SV_Target {
    float3 lightDir = normalize(float3(0.5, 1.0, 0.3));
    float ndotl = max(dot(input.normal, lightDir), 0.0);
    float ambient = 0.15;
    float lighting = ambient + ndotl * 0.85;

    float4 baseColor = input.color;
    return float4(baseColor.rgb * lighting, baseColor.a);
}
