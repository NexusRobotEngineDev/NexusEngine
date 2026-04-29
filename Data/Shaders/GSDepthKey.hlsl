struct GaussianSplat {
    float4 pos_opacity;
    float4 rot;
    float4 scale;
    float4 color;
};

struct PushConstants {
    float4x4 view;
    uint splatCount;
    uint paddedCount;
};
[[vk::push_constant]] ConstantBuffer<PushConstants> pc;

StructuredBuffer<GaussianSplat> splats : register(t0, space0);
RWStructuredBuffer<uint2> sortKeysAndValues : register(u1, space0);

[numthreads(256, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID) {
    uint idx = DTid.x;

    if (idx >= pc.paddedCount) return;

    if (idx >= pc.splatCount) {
        sortKeysAndValues[idx] = uint2(0xFFFFFFFF, 0);
        return;
    }

    float3 pos_raw = splats[idx].pos_opacity.xyz;
    float3 pos = float3(pos_raw.x, pos_raw.z, -pos_raw.y);
    float4 viewPos = mul(pc.view, float4(pos, 1.0));
    float opacity = 1.0 / (1.0 + exp(-splats[idx].pos_opacity.w));

    if (viewPos.z >= -0.2 || opacity < 0.005) {
        sortKeysAndValues[idx] = uint2(0xFFFFFFFF, idx);
        return;
    }

    float depth = -viewPos.z;
    uint depthKey = ~asuint(depth);
    sortKeysAndValues[idx] = uint2(depthKey, idx);
}
