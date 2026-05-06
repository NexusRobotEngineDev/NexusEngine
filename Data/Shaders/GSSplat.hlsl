struct GaussianSplat {
    float4 pos_opacity;
    float4 rot;
    float4 scale;
    float4 color;
};

struct PushConstants {
    float4x4 viewProj;
    float4x4 view;
    float4x4 proj;
    float2 viewport;
    float focalX;
    float focalY;
};

[[vk::push_constant]] ConstantBuffer<PushConstants> ubo;

StructuredBuffer<GaussianSplat> splats : register(t0, space0);
StructuredBuffer<uint2> sortKeysAndValues : register(t1, space0);

struct VSInput {
    uint vertexID : SV_VertexID;
    uint instanceID : SV_InstanceID;
};

struct VSOutput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
    float4 conicOpacity : TEXCOORD1;
};

static const float2 quadOffsets[4] = {
    float2(-1.0, -1.0),
    float2( 1.0, -1.0),
    float2(-1.0,  1.0),
    float2( 1.0,  1.0)
};

static const float3x3 R_COORD = float3x3(
    1,  0,  0,
    0, -1,  0,
    0,  0, -1
);

VSOutput vsMain(VSInput input) {
    VSOutput output;
    output.uv = float2(0,0);
    output.color = float4(0,0,0,0);
    output.conicOpacity = float4(0,0,0,0);

    uint sortedIdx = input.instanceID;
    uint splatIdx = sortKeysAndValues[sortedIdx].y;
    GaussianSplat splat = splats[splatIdx];

    float qlen = length(splat.rot);
    float4 q = qlen > 0.0001 ? splat.rot / qlen : float4(1, 0, 0, 0);
    splat.rot = q;

    float3 pos_raw = splat.pos_opacity.xyz;
    float3 pos_w = mul(R_COORD, pos_raw);

    float4 clipPos = mul(ubo.viewProj, float4(pos_w, 1.0));

    if (clipPos.w <= 0.01) {
        output.position = float4(0, 0, -2, 1);
        return output;
    }

    float2 ndc = clipPos.xy / clipPos.w;
    if (abs(ndc.x) > 1.3 || abs(ndc.y) > 1.3) {
        output.position = float4(0, 0, -2, 1);
        return output;
    }

    float3 pos_v = mul(ubo.view, float4(pos_w, 1.0)).xyz;
    float tz = max(-pos_v.z, 0.1);
    float tz2 = tz * tz;

    float fx = ubo.focalX;
    float fy = ubo.focalY;

    float3x3 J = float3x3(
        fx / tz,   0.0,       fx * pos_v.x / tz2,
        0.0,       fy / tz,   fy * pos_v.y / tz2,
        0.0,       0.0,       0.0
    );

    float3x3 W = (float3x3)ubo.view;

    float4 q_rot = splat.rot;
    float r = q_rot.x, x = q_rot.y, y = q_rot.z, z = q_rot.w;
    float3x3 Rot = float3x3(
        1.0 - 2.0*(y*y + z*z), 2.0*(x*y - r*z),       2.0*(x*z + r*y),
        2.0*(x*y + r*z),       1.0 - 2.0*(x*x + z*z), 2.0*(y*z - r*x),
        2.0*(x*z - r*y),       2.0*(y*z + r*x),       1.0 - 2.0*(x*x + y*y)
    );

    float3 s = exp(splat.scale.xyz);
    float3x3 S = float3x3(
        s.x, 0, 0,
        0, s.y, 0,
        0, 0, s.z
    );

    float3x3 RotYUp = mul(R_COORD, Rot);
    float3x3 M3d = mul(RotYUp, S);
    float3x3 Sigma3D = mul(M3d, transpose(M3d));

    float3x3 T = mul(J, W);
    float3x3 cov2D = mul(mul(T, Sigma3D), transpose(T));

    float a = cov2D[0][0] + 0.3;
    float b = cov2D[0][1];
    float c = cov2D[1][1] + 0.3;

    float det = a * c - b * b;
    if (det <= 0.0) det = 0.001;
    float invDet = 1.0 / det;

    float3 conic = float3(c * invDet, -b * invDet, a * invDet);

    float midVal = 0.5 * (a + c);
    float lambda1 = midVal + sqrt(max(midVal * midVal - det, 0.0));
    float lambda2 = midVal - sqrt(max(midVal * midVal - det, 0.0));
    float radius = ceil(3.0 * sqrt(max(lambda1, lambda2)));
    radius = clamp(radius, 1.0, 512.0);

    float2 offset = quadOffsets[input.vertexID] * radius;
    output.uv = offset;

    clipPos.xy += offset / ubo.viewport * clipPos.w * 2.0;
    output.position = clipPos;

    float SH_C0 = 0.28209479177387814;
    float3 rgb = max(splat.color.rgb * SH_C0 + 0.5, 0.0);

    float sigmoidOpac = 1.0 / (1.0 + exp(-splat.pos_opacity.w));
    output.color = float4(rgb, sigmoidOpac);
    output.conicOpacity = float4(conic, sigmoidOpac);

    return output;
}

float4 psMain(VSOutput input) : SV_Target {
    float2 d = input.uv;
    float3 conic = input.conicOpacity.xyz;
    float opacity = input.conicOpacity.w;

    float power = -0.5 * (conic.x * d.x * d.x + 2.0 * conic.y * d.x * d.y + conic.z * d.y * d.y);
    if (power > 0.0) power = 0.0;

    float alpha = exp(power) * opacity;
    if (alpha < 1.0 / 255.0) discard;
    alpha = min(alpha, 0.99);

    return float4(input.color.rgb * alpha, alpha);
}
