#language slang 2026

module sky;
import common;
import global_desc;

static const float PI = 3.14159265359;

public float4 main_sky(VertexOut vo, PushConstants pc) {
  // vo.uv1 ranges from (-1, -1) to (1, 1) across the bottom-left subwindow
  float2 p = vo.uv1;
  p.x *= pc.skyAspect;
  float tanHalfFov = tan(pc.fov * 0.5f);
  float3 rayDir = normalize(pc.camForward + p.x * tanHalfFov * pc.camRight - p.y * tanHalfFov * pc.camUp);
  float3 rayOrigin = pc.camPos;

  // Off-white background
  float3 offWhite = float3(0.94f, 0.94f, 0.92f);

  // Ray - Celestial Sphere intersection (center at (0,0,0), radius = 1.0)
  float R = 1.0f;
  float b = dot(rayOrigin, rayDir);
  float c = dot(rayOrigin, rayOrigin) - R * R;
  float h = b * b - c;

  if (h >= 0.0f) {
    float t = -b - sqrt(h);
    if (t > 0.0f) {
      float3 hitPos = rayOrigin + t * rayDir;
      float3 N = normalize(hitPos);

      // Outside view of the celestial globe (inverting longitude so constellations face outward)
      float skyU = frac(0.5f - atan2(N.z, N.x) / (2.0f * PI));
      if (skyU < 0.0f) skyU += 1.0f;
      float skyV = 0.5f - asin(clamp(N.y, -1.0f, 1.0f)) / PI;

      float3 starCol = (pc.skyTexId != 0xFFFFFFFF)
          ? sample2DLinear(pc.skyTexId, float2(skyU, skyV)).rgb
          : float3(0.05f, 0.05f, 0.1f);

      // Smooth silhouette anti-aliasing against the off-white background
      float edgeAlpha = smoothstep(0.0f, 0.008f, sqrt(h));
      return float4(lerp(offWhite, starCol, edgeAlpha), 1.0f);
    }
  }

  return float4(offWhite, 1.0f);
}

