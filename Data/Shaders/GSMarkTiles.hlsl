struct PushConstants {
    uint totalInstances;
};
[[vk::push_constant]] ConstantBuffer<PushConstants> pc;

StructuredBuffer<uint2> sortKeysAndValues : register(u1, space0);
RWStructuredBuffer<uint2> tileRanges : register(u10, space0);

[numthreads(256, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID) {
    uint idx = DTid.x;
    if (idx >= pc.totalInstances) return;

    uint currTile = sortKeysAndValues[idx].x >> 16;
    uint prevTile = (idx == 0) ? 0xFFFFFFFF : (sortKeysAndValues[idx - 1].x >> 16);

    if (currTile != prevTile) {
        if (currTile != 0xFFFF) {
            tileRanges[currTile].x = idx;
        }
        if (idx > 0 && prevTile != 0xFFFF) {
            tileRanges[prevTile].y = idx;
        }
    }

    if (currTile != 0xFFFF && (idx == pc.totalInstances - 1 || sortKeysAndValues[idx + 1].x >> 16 == 0xFFFF)) {
        tileRanges[currTile].y = idx + 1;
    }
}
