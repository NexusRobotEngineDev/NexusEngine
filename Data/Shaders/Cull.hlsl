struct ObjectData {
    uint textureIndex;
    uint normalIndex;
    uint metallicRoughnessIndex;
    uint occlusionIndex;
    uint emissiveIndex;
    uint samplerIndex;
    uint isVisible;
    uint _pad0;

    float4 albedoFactor;
    float metallicFactor;
    float roughnessFactor;
    float2 padding;

    float4x4 mvp;
    float4x4 worldMatrix;
    float4 highlightColor;
};

struct DrawIndexedIndirectCommand {
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    int  vertexOffset;
    uint firstInstance;
};

StructuredBuffer<ObjectData> objects : register(t2, space0);

StructuredBuffer<DrawIndexedIndirectCommand> drawCommandsIn : register(t0, space1);
RWStructuredBuffer<DrawIndexedIndirectCommand> drawCommandsOut : register(u1, space1);
RWStructuredBuffer<uint> drawCountBuffer : register(u2, space1);

struct PushParams {
    uint maxEntities;
    uint frameIndex;
    uint frameOffset;
};

[[vk::push_constant]]
ConstantBuffer<PushParams> pushParams;

[numthreads(64, 1, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID) {
    uint entityIndex = DTid.x;

    if (entityIndex >= pushParams.maxEntities) {
        return;
    }

    uint objIndex = pushParams.frameOffset + entityIndex;

    if (objects[objIndex].isVisible != 0) {
        uint outputIndex;
        InterlockedAdd(drawCountBuffer[pushParams.frameIndex], 1, outputIndex);

        drawCommandsOut[outputIndex] = drawCommandsIn[entityIndex];
    }
}
