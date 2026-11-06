#language slang 2026

module sky;
import common;

public float4 main_sky(VertexOut vo) {
  // Empty for now
  return float4(vo.color, 1.0f);
}

public float4 main_sky(VertexOut vo, DrawInfo info) {
  return main_sky(vo);
}
