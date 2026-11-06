#language slang 2026

module orbit;
import common;
import global_desc;

static const float PI = 3.14159265359;

public float4 main_orbit(VertexOut vo, PushConstants pc) {
  // vo.uv1 ranges from (-1, -1) bottom-left/top-left to (1, 1) across the subwindow
  float2 p = vo.uv1;
  p.x *= pc.aspect;
  float tanHalfFov = tan(pc.fov * 0.5f);
  float3 rayDir = normalize(pc.camForward + p.x * tanHalfFov * pc.camRight - p.y * tanHalfFov * pc.camUp);
  float3 rayOrigin = pc.camPos;

  // Background starmap calculation (spherical lookup for rayDir)
  float skyU = 0.5f + atan2(rayDir.z, rayDir.x) / (2.0f * PI);
  float skyV = 0.5f - asin(clamp(rayDir.y, -1.0f, 1.0f)) / PI;
  float3 skyColor = (pc.skyTexId != 0xFFFFFFFF)
      ? sample2DLinear(pc.skyTexId, float2(skyU, skyV)).rgb * 0.45f
      : float3(0.02f, 0.02f, 0.04f);

  float3 finalColor = skyColor;

  // 1. Ray - Earth Sphere intersection (center at (0,0,0), radius = pc.earthRadius)
  float R = pc.earthRadius;
  float b = dot(rayOrigin, rayDir);
  float c = dot(rayOrigin, rayOrigin) - R * R;
  float h = b * b - c;
  float tEarth = -1.0f;

  if (h >= 0.0f) {
    float t = -b - sqrt(h);
    if (t > 0.0f) {
      tEarth = t;
      float3 hitPos = rayOrigin + t * rayDir;
      float3 N = normalize(hitPos);

      // Spherical equirectangular UV mapping for Earth
      float earthU = 0.5f + atan2(N.z, N.x) / (2.0f * PI);
      float earthV = 0.5f - asin(clamp(N.y, -1.0f, 1.0f)) / PI;

      float3 earthAlbedo = (pc.earthTexId != 0xFFFFFFFF)
          ? sample2DLinear(pc.earthTexId, float2(earthU, earthV)).rgb
          : float3(0.1f, 0.4f, 0.8f);

      // Directional sun lighting + ambient
      float3 sunDir = normalize(float3(1.0f, 0.5f, 0.8f));
      float diff = max(0.08f, dot(N, sunDir));

      // Atmospheric limb glow
      float rim = 1.0f - max(0.0f, dot(-rayDir, N));
      float3 atmosphere = float3(0.2f, 0.5f, 1.0f) * pow(rim, 3.5f) * 0.8f;

      finalColor = earthAlbedo * diff + atmosphere;
    }
  }

  // 2. Draw Orbit Rings and Satellites
  if (pc.satelliteBuffer != nullptr && pc.numSatellites > 0) {
    for (uint32_t i = 0; i < pc.numSatellites; ++i) {
      Satellite sat = pc.satelliteBuffer[i];

      // Convert (theta, phi) spherical angles to 3D position
      float cosPhi = cos(sat.phi);
      float sinPhi = sin(sat.phi);
      float cosTheta = cos(sat.theta);
      float sinTheta = sin(sat.theta);
      float3 satPos = sat.radius * float3(cosPhi * sinTheta, sinPhi, cosPhi * cosTheta);

      // Project ray to satellite point
      float3 toSat = satPos - rayOrigin;
      float tSat = dot(toSat, rayDir);
      if (tSat > 0.0f) {
        // Occlusion check against Earth
        if (tEarth < 0.0f || tSat < tEarth) {
          float3 closest = rayOrigin + tSat * rayDir;
          float distToRay = length(satPos - closest);

          // Glowing satellite dot
          float satCore = smoothstep(0.035f, 0.005f, distToRay);
          float satGlow = smoothstep(0.12f, 0.01f, distToRay) * 0.4f;

          float3 satCol = float3(1.0f, 0.95f, 0.4f);
          finalColor = lerp(finalColor, satCol, satCore + satGlow);
        }
      }
    }
  }

  return float4(finalColor, 1.0f);
}
