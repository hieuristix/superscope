#language slang 2026

module raster;
import global_desc;

struct DrawInfo {
  column_major float3x3 transform;
  float4 uvBounds; // left top right bottom
  float3 color;
  uint32_t texID;
  uint32_t isMSDF;
};

struct PushConstants {
  DrawInfo* infoBuffer;
};

[[vk::push_constant]]
uniform PushConstants pc;

struct VertexIn {
  uint32_t vertID : SV_VertexID;
  uint32_t instID : SV_InstanceID;
};

struct VertexOut {
  uint32_t instID : SV_InstanceID;
  float4 pos   : SV_Position;
  float2 uv    : TEXCOORD0;
  float2 uv1   : TEXCOORD1;
  float3 color : TEXCOORD2;
};

static const float2 billboardCorners[3] = {
    float2(-1.0, -1.0),
    float2( 3.0, -1.0),
    float2(-1.0,  3.0)
};

static const float3 billboardColors[3] = {
    float3(1.0, 0.0, 0.0),
    float3(0.0, 1.0, 0.0),
    float3(0.0, 0.0, 1.0)
};

[shader("vertex")]
VertexOut main(VertexIn vi) {
  float2 pos = billboardCorners[vi.vertID];
  DrawInfo info = pc.infoBuffer[vi.instID];

  VertexOut vo;
  vo.instID = vi.instID;
  float3 screenPos = mul(info.transform, float3(pos, 1.0f));
  vo.pos = float4(screenPos.xy, 0.0f, 1.0f);
  float2 normalized_uv = pos * 0.5f + 0.5f; // DO NOT CLAMP
  vo.uv = normalized_uv * (info.uvBounds.zw - info.uvBounds.xy) + info.uvBounds.xy;
  vo.uv1 = pos;
  vo.color = billboardColors[vi.vertID];
  return vo;
}

[shader("fragment")]
float4 main(VertexOut vo) : SV_Target {
  if (abs(vo.uv1.x) > 1.0 || abs(vo.uv1.y) > 1.0) {
    discard;
  }

  float4 frag_out;
  DrawInfo info = pc.infoBuffer[vo.instID];

  float4 texel = (info.texID == 0xFFFFFFFF)
    ? float4(vo.color, 1.0f)
    : sample2DLinear(info.texID, vo.uv);

  if (info.isMSDF != 0) {
    float signDist = median3(texel.r, texel.g, texel.b);
    float w = fwidth(signDist);
    float opacity = smoothstep(0.5 - w, 0.5 + w, signDist);
    frag_out = float4(float3(1.0), opacity);
  } else {
    frag_out = texel;
  }

  return frag_out;
}
