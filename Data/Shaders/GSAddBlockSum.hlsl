struct PushConstants {
    uint numElements;
};
[[vk::push_constant]] ConstantBuffer<PushConstants> pc;

RWStructuredBuffer<uint> dataOut : register(u3, space0);
RWStructuredBuffer<uint> blockSums : register(u4, space0);

[numthreads(512, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID) {
    uint idx = DTid.x;
    if (idx < pc.numElements) {
        dataOut[idx] += blockSums[idx >> 10];
    }
}
