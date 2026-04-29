struct GaussianProjected {
    float2 center;
    float depth;
    float opacity;
    float3 conic;
    float4 color;
    uint2 boundsMinMax;
};

struct PushConstants {
    uint2 gridDims;
};
[[vk::push_constant]] ConstantBuffer<PushConstants> pc;

StructuredBuffer<uint2> sortKeysAndValues : register(u1, space0);
StructuredBuffer<GaussianProjected> projected : register(u7, space0);
StructuredBuffer<uint2> tileRanges : register(u10, space0);
RWTexture2D<float4> outputRT : register(u11, space0);

groupshared GaussianProjected localSplats[256];
groupshared uint blockActive;

[numthreads(16, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint3 GID : SV_GroupID) {
    uint2 px = DTid.xy;
    uint localIdx = GTid.y * 16 + GTid.x;
    uint tileID = GID.y * pc.gridDims.x + GID.x;

    uint2 range = tileRanges[tileID];
    uint startIdx = range.x;
    uint endIdx = range.y;

    float3 colorAcc = float3(0.0f, 0.0f, 0.0f);
    float T = 0.0f;

    uint numRounds = (endIdx - startIdx + 255) / 256;
    for (uint i = 0; i < numRounds; i++) {
        uint fetchIdx = startIdx + i * 256 + localIdx;
        if (fetchIdx < endIdx) {
            uint splatID = sortKeysAndValues[fetchIdx].y;
            localSplats[localIdx] = projected[splatID];
        }

        if (localIdx == 0) {
            blockActive = 0;
        }
        GroupMemoryBarrierWithGroupSync();

        uint elementsInBlock = min(256u, endIdx - (startIdx + i * 256));

        for (uint j = 0; j < elementsInBlock; j++) {
            if (T >= 0.999f) continue;

            GaussianProjected s = localSplats[j];

            float2 d = float2(px) - s.center;
            float power = -0.5f * (s.conic.x * d.x * d.x + s.conic.z * d.y * d.y) - s.conic.y * d.x * d.y;

            if (power > 0.0f || power < -10.0f) continue;

            float alpha = min(0.999f, s.opacity * exp(power));
            if (alpha < 1.0f / 255.0f) continue;

            float test_alpha = alpha * (1.0f - T);
            colorAcc += test_alpha * s.color.rgb;
            T += test_alpha;
        }

        if (T < 0.99f) {
            InterlockedAdd(blockActive, 1);
        }
        GroupMemoryBarrierWithGroupSync();
        if (blockActive == 0) {
            break;
        }
    }

    outputRT[px] = float4(colorAcc, T);
}
