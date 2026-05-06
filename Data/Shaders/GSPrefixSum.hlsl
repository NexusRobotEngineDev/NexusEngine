struct UintElement { uint val; };

struct PushConstants {
    uint numElements;
    uint writeBlockSum;
};
[[vk::push_constant]] ConstantBuffer<PushConstants> pc;

RWStructuredBuffer<UintElement> dataOut : register(u3, space0);
RWStructuredBuffer<UintElement> blockSums : register(u4, space0);

groupshared uint temp[1024];

[numthreads(512, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint3 GID : SV_GroupID) {
    uint n = pc.numElements;
    uint idx0 = GID.x * 1024 + GTid.x * 2;
    uint idx1 = idx0 + 1;

    temp[GTid.x * 2]     = (idx0 < n) ? dataOut[idx0].val : 0;
    temp[GTid.x * 2 + 1] = (idx1 < n) ? dataOut[idx1].val : 0;
    GroupMemoryBarrierWithGroupSync();

    uint offset = 1;
    for (uint d = 1024 >> 1; d > 0; d >>= 1) {
        GroupMemoryBarrierWithGroupSync();
        if (GTid.x < d) {
            uint ai = offset * (2 * GTid.x + 1) - 1;
            uint bi = offset * (2 * GTid.x + 2) - 1;
            temp[bi] += temp[ai];
        }
        offset *= 2;
    }

    GroupMemoryBarrierWithGroupSync();
    if (GTid.x == 0) {
        if (pc.writeBlockSum == 1 && GID.x < ((n + 1023) / 1024)) {
            blockSums[GID.x].val = temp[1023];
        }
        temp[1023] = 0;
    }

    for (uint d2 = 1; d2 < 1024; d2 *= 2) {
        offset >>= 1;
        GroupMemoryBarrierWithGroupSync();
        if (GTid.x < d2) {
            uint ai = offset * (2 * GTid.x + 1) - 1;
            uint bi = offset * (2 * GTid.x + 2) - 1;
            uint t = temp[ai];
            temp[ai] = temp[bi];
            temp[bi] += t;
        }
    }
    GroupMemoryBarrierWithGroupSync();

    if (idx0 < n) dataOut[idx0].val = temp[GTid.x * 2];
    if (idx1 < n) dataOut[idx1].val = temp[GTid.x * 2 + 1];
}
