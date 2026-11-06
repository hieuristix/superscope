#language slang 2026

module sky;
import common;
import global_desc;

public float4 main_sky(VertexOut vo, PushConstants pc) {
  if (pc.skyTexId != 0xFFFFFFFF) {
    // Equirectangular sky map
    float3 col = sample2DLinear(pc.skyTexId, vo.uv).rgb;
    return float4(col, 1.0f);
  }
  return float4(vo.color, 1.0f);
}
