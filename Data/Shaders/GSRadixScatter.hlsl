struct UintElement { uint val; };

struct PushConstants {
    uint splatCount;
    uint shift;
    uint blockCount;
    uint elementsPerBlock;
};
[[vk::push_constant]] ConstantBuffer<PushConstants> pc;

RWStructuredBuffer<uint2> keysValuesIn : register(u1, space0);
RWStructuredBuffer<UintElement> globalOffsets : register(u3, space0);
RWStructuredBuffer<uint2> keysValuesOut : register(u2, space0);

groupshared uint bucketWriteOffset[256];

[numthreads(256, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint3 GID : SV_GroupID) {
    if (GID.x < pc.blockCount) {
        bucketWriteOffset[GTid.x] = globalOffsets[GTid.x * pc.blockCount + GID.x].val;
    }
    GroupMemoryBarrierWithGroupSync();

    uint startIdx = GID.x * pc.elementsPerBlock;
    uint endIdx = min(startIdx + pc.elementsPerBlock, pc.splatCount);

    for (uint i = startIdx + GTid.x; i < endIdx; i += 256) {
        uint2 kv = keysValuesIn[i];
        uint bucket = (kv.x >> pc.shift) & 0xFF;

        uint insertIdx;
        InterlockedAdd(bucketWriteOffset[bucket], 1, insertIdx);

        keysValuesOut[insertIdx] = kv;
    }
}
