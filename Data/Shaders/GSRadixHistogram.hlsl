struct UintElement { uint val; };

struct PushConstants {
    uint splatCount;
    uint shift;
    uint blockCount;
    uint elementsPerBlock;
};
[[vk::push_constant]] ConstantBuffer<PushConstants> pc;

RWStructuredBuffer<uint2> keysValuesIn : register(u1, space0);
RWStructuredBuffer<UintElement> globalHistograms : register(u3, space0);

groupshared uint localHist[256];

[numthreads(256, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint3 GID : SV_GroupID) {
    localHist[GTid.x] = 0;
    GroupMemoryBarrierWithGroupSync();

    uint startIdx = GID.x * pc.elementsPerBlock;
    uint endIdx = min(startIdx + pc.elementsPerBlock, pc.splatCount);

    for (uint i = startIdx + GTid.x; i < endIdx; i += 256) {
        uint key = keysValuesIn[i].x;
        uint bucket = (key >> pc.shift) & 0xFF;
        InterlockedAdd(localHist[bucket], 1);
    }

    GroupMemoryBarrierWithGroupSync();

    if (GID.x < pc.blockCount) {
        globalHistograms[GTid.x * pc.blockCount + GID.x].val = localHist[GTid.x];
    }
}
