#language slang 2026

module capture;
import common;

public float4 main_capture(VertexOut vo) {
  // Empty for now
  return float4(vo.color, 1.0f);
}

public float4 main_capture(VertexOut vo, DrawInfo info) {
  return main_capture(vo);
}
