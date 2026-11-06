#language slang 2026

module raster;
import global_desc;
import common;
import capture;
import sky;
import orbit;

struct PushConstants {
  DrawInfo* infoBuffer;
};

[[vk::push_constant]]
uniform PushConstants pc;

struct VertexIn {
  uint32_t vertID : SV_VertexID;
  uint32_t instID : SV_InstanceID;
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
  float2 normalized_uv = pos * 0.5f + 0.5f;
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

  DrawInfo info = pc.infoBuffer[vo.instID];

  // Top left: main_capture (instID 0)
  // Bot left: main_sky     (instID 1)
  // Right:    main_orbit   (instID 2)
  if (vo.instID == 0) {
    return main_capture(vo, info);
  } else if (vo.instID == 1) {
    return main_sky(vo, info);
  } else if (vo.instID == 2) {
    return main_orbit(vo, info);
  }

  return float4(vo.color, 1.0f);
}
