struct GaussianProjected {
    float2 center;
    float depth;
    float opacity;
    float3 conic;
    float4 color;
    uint2 boundsMinMax;
};

struct PushConstants {
    uint splatCount;
    uint gridWidth;
    uint maxInstances;
};
[[vk::push_constant]] ConstantBuffer<PushConstants> pc;

StructuredBuffer<GaussianProjected> projected : register(u7, space0);
StructuredBuffer<uint> overlapCounts : register(u8, space0);
StructuredBuffer<uint> overlapPrefixSums : register(u9, space0);

RWStructuredBuffer<uint2> sortKeysAndValues : register(u1, space0);

uint floatToHalf(float f) {
    uint u = asuint(f);
    uint sign = (u >> 16) & 0x8000;
    int exp = ((u >> 23) & 0xFF) - 127 + 15;
    uint mant = u & 0x7FFFFF;

    if (exp <= 0) {
        return sign;
    } else if (exp >= 31) {
        return sign | 0x7C00;
    }
    return sign | (exp << 10) | (mant >> 13);
}

[numthreads(256, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID) {
    uint idx = DTid.x;
    if (idx >= pc.splatCount) return;

    uint count = overlapCounts[idx];
    if (count == 0) return;

    uint baseOffset = overlapPrefixSums[idx];

    GaussianProjected proj = projected[idx];
    uint minX = proj.boundsMinMax.x & 0xFFFF;
    uint minY = proj.boundsMinMax.x >> 16;
    uint maxX = proj.boundsMinMax.y & 0xFFFF;
    uint maxY = proj.boundsMinMax.y >> 16;

    uint depth16 = floatToHalf(proj.depth);

    uint currentOffset = baseOffset;
    for (uint y = minY; y <= maxY; y++) {
        for (uint x = minX; x <= maxX; x++) {
            uint tileID = y * pc.gridWidth + x;
            uint key = (tileID << 16) | (depth16 & 0xFFFF);
            if (currentOffset < pc.maxInstances) {
                sortKeysAndValues[currentOffset] = uint2(key, idx);
            }
            currentOffset++;
        }
    }
}
