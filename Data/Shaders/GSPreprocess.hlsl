struct GaussianSplat {
    float4 pos_opacity;
    float4 rot;
    float4 scale;
    float4 color;
};

struct GaussianProjected {
    float2 center;
    float depth;
    float opacity;
    float3 conic;
    float4 color;
    uint2 boundsMinMax;
};

struct PushConstants {
    float4x4 viewProj;
    float4x4 view;
    float3 cameraPos;
    uint splatCount;
    uint2 gridDims;
    float2 viewport;
    float focalX;
    float focalY;
};
[[vk::push_constant]] ConstantBuffer<PushConstants> pc;

struct UintElement { uint val; };

StructuredBuffer<GaussianSplat> splats : register(t0, space0);
RWStructuredBuffer<GaussianProjected> projected : register(u7, space0);
RWStructuredBuffer<UintElement> overlapCounts : register(u8, space0);
RWStructuredBuffer<UintElement> overlapPrefixSums : register(u9, space0);
RWStructuredBuffer<UintElement> indirectDrawCount : register(u6, space0);

void computeCov3D(float4 rot, float4 scale, out float3 cov3D[3]) {
    float3x3 S = float3x3(
        exp(scale.x), 0.0f, 0.0f,
        0.0f, exp(scale.y), 0.0f,
        0.0f, 0.0f, exp(scale.z)
    );

    float r = rot.x, x = rot.y, y = rot.z, z = rot.w;
    float3x3 R = float3x3(
        1.0 - 2.0 * (y*y + z*z), 2.0 * (x*y - r*z),       2.0 * (x*z + r*y),
        2.0 * (x*y + r*z),       1.0 - 2.0 * (x*x + z*z), 2.0 * (y*z - r*x),
        2.0 * (x*z - r*y),       2.0 * (y*z + r*x),       1.0 - 2.0 * (x*x + y*y)
    );

    float3x3 M = mul(R, S);
    float3x3 Sigma = mul(M, transpose(M));

    cov3D[0] = float3(Sigma[0][0], Sigma[0][1], Sigma[0][2]);
    cov3D[1] = float3(Sigma[1][0], Sigma[1][1], Sigma[1][2]);
    cov3D[2] = float3(Sigma[2][0], Sigma[2][1], Sigma[2][2]);
}

float3 computeCov2D(float3 p, float3 cov3D[3]) {
    float4 p_view = mul(pc.view, float4(p, 1.0f));

    float x = p_view.x;
    float y = p_view.y;
    float z = p_view.z;

    float tx = x / z;
    float ty = y / z;

    float3x3 J = float3x3(
        pc.focalX / z, 0.0f,          -(pc.focalX * x) / (z * z),
        0.0f,          pc.focalY / z, -(pc.focalY * y) / (z * z),
        0.0f,          0.0f,          0.0f
    );

    float3x3 W = float3x3(
        pc.view[0][0], pc.view[1][0], pc.view[2][0],
        pc.view[0][1], pc.view[1][1], pc.view[2][1],
        pc.view[0][2], pc.view[1][2], pc.view[2][2]
    );

    float3x3 T = mul(J, W);
    float3x3 cov3D_mat = float3x3(cov3D[0], cov3D[1], cov3D[2]);

    float3x3 cov2D_mat = mul(T, mul(cov3D_mat, transpose(T)));

    cov2D_mat[0][0] += 0.3f;
    cov2D_mat[1][1] += 0.3f;

    return float3(cov2D_mat[0][0], cov2D_mat[0][1], cov2D_mat[1][1]);
}

[numthreads(256, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID) {
    uint idx = DTid.x;
    if (idx >= pc.splatCount) return;

    GaussianSplat s = splats[idx];
    float3 p = s.pos_opacity.xyz;
    float opacity = 1.0f / (1.0f + exp(-s.pos_opacity.w));
    float4 p_view = mul(pc.view, float4(p, 1.0f));

    if (p_view.z >= -0.2f || opacity < 0.005f) {
        overlapCounts[idx] = 0;
        return;
    }

    float4 p_proj = mul(pc.viewProj, float4(p, 1.0f));
    float2 ndc = p_proj.xy / p_proj.w;
    float2 center = (ndc * 0.5f + 0.5f) * pc.viewport;

    float3 cov3D[3];
    computeCov3D(s.rot, s.scale, cov3D);
    float3 cov2D = computeCov2D(p, cov3D);

    float det = (cov2D.x * cov2D.z - cov2D.y * cov2D.y);
    if (det < 1e-5) det = 1e-5;
    float det_inv = 1.0f / det;
    float3 conic = float3(cov2D.z * det_inv, -cov2D.y * det_inv, cov2D.x * det_inv);

    float mid = 0.5f * (cov2D.x + cov2D.z);
    float lambda1 = mid + sqrt(max(0.1f, mid * mid - det));
    float lambda2 = mid - sqrt(max(0.1f, mid * mid - det));
    float radius = ceil(3.0f * sqrt(max(lambda1, lambda2)));

    float2 min_xy = center - radius;
    float2 max_xy = center + radius;

    if (max_xy.x < 0.0f || max_xy.y < 0.0f || min_xy.x > pc.viewport.x || min_xy.y > pc.viewport.y) {
        overlapCounts[idx] = 0;
        return;
    }

    uint rectMinX = min(pc.gridDims.x - 1, max(0u, (uint)(min_xy.x / 16.0f)));
    uint rectMinY = min(pc.gridDims.y - 1, max(0u, (uint)(min_xy.y / 16.0f)));
    uint rectMaxX = min(pc.gridDims.x - 1, max(0u, (uint)(max_xy.x / 16.0f)));
    uint rectMaxY = min(pc.gridDims.y - 1, max(0u, (uint)(max_xy.y / 16.0f)));

    uint overlapCount = (rectMaxX - rectMinX + 1) * (rectMaxY - rectMinY + 1);

    if(overlapCount > 256) overlapCount = 256;

    uint offset;
    InterlockedAdd(indirectDrawCount[0].val, overlapCount, offset);
    overlapPrefixSums[idx].val = offset;
    overlapCounts[idx].val = overlapCount;

    GaussianProjected proj;
    proj.center = center;
    proj.depth = -p_view.z;
    proj.opacity = opacity;
    proj.conic = conic;
    proj.color = float4(max(0.0f, s.color.x), max(0.0f, s.color.y), max(0.0f, s.color.z), 1.0f);
    proj.boundsMinMax = uint2((rectMinY << 16) | rectMinX, (rectMaxY << 16) | rectMaxX);

    projected[idx] = proj;
    overlapCounts[idx].val = overlapCount;
}
