#language slang 2026

module capture;
import common;

public float4 main_capture(VertexOut vo, PushConstants pc) {
  // Empty for now: returns interpolated corner colors
  return float4(vo.color, 1.0f);
}
