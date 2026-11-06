#language slang 2026

module orbit;
import common;

public float4 main_orbit(VertexOut vo) {
  // Empty for now
  return float4(vo.color, 1.0f);
}

public float4 main_orbit(VertexOut vo, DrawInfo info) {
  return main_orbit(vo);
}
