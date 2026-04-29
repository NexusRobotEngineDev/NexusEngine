
struct PushConstants {
    uint splatCount;
    uint stage;
    uint passOfStage;
};

[[vk::push_constant]] ConstantBuffer<PushConstants> ubo;

RWStructuredBuffer<uint2> sortKeysAndValues : register(u1, space0);

[numthreads(256, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID) {
    uint i = DTid.x;
    if (i >= ubo.splatCount / 2) return;

    uint j = ubo.stage;
    uint k = ubo.passOfStage;

    uint chunkSize = 1u << (k + 1);
    uint chunkStride = 1u << k;

    uint indexInChunk = i % chunkStride;
    uint chunkIndex = i / chunkStride;

    uint idx1 = (chunkIndex * chunkSize) + indexInChunk;
    uint idx2 = idx1 + chunkStride;

    uint sortDir = ((idx1 & (1u << (j + 1))) == 0) ? 1 : 0;

    uint2 val1 = sortKeysAndValues[idx1];
    uint2 val2 = sortKeysAndValues[idx2];

    bool condition = (val1.x > val2.x);
    if (sortDir == 0) condition = !condition;

    if (condition) {
        sortKeysAndValues[idx1] = val2;
        sortKeysAndValues[idx2] = val1;
    }
}
