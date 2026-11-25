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

  float tNearest = 1e20f;
  float tEarth = -1.0f;
  float3 finalColor = skyColor;

  // 1. Ray - Earth Sphere intersection (center at (0,0,0), radius = pc.earthRadius)
  float R = pc.earthRadius;
  float bEarth = dot(rayOrigin, rayDir);
  float cEarth = dot(rayOrigin, rayOrigin) - R * R;
  float hEarth = bEarth * bEarth - cEarth;

  if (hEarth >= 0.0f) {
    float t = -bEarth - sqrt(hEarth);
    if (t > 0.0f && t < tNearest) {
      tNearest = t;
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

  // 2. Draw Orbit Lines following a plane slice of the Earth
  if (pc.satelliteBuffer != nullptr && pc.numSatellites > 0) {
    for (uint32_t i = 0; i < pc.numSatellites; ++i) {
      Satellite sat = pc.satelliteBuffer[i];
      float3 N = sat.planeNormal;
      float R = sat.radius;

      // 1. Bounding sphere test: strictly reject any rays heading into the skybox
      float R_bound = R + 0.15f;
      float bBound = dot(rayOrigin, rayDir);
      float cBound = dot(rayOrigin, rayOrigin) - R_bound * R_bound;
      float disc = bBound * bBound - cBound;
      if (disc < 0.0f) continue;

      float sqrtDisc = sqrt(disc);
      float tEnter = max(0.0f, -bBound - sqrtDisc);
      float tExit = -bBound + sqrtDisc;
      if (tExit <= 0.0f) continue;

      float distPerp = 1e10f;
      float tHit = 0.0f;
      bool hitFound = false;

      // 2. Plane intersection
      float denom = dot(N, rayDir);
      if (abs(denom) > 1e-4f) {
        float tPlane = -dot(N, rayOrigin) / denom;
        if (tPlane >= tEnter && tPlane <= tExit) {
          float3 pPlane = rayOrigin + tPlane * rayDir;
          float dPlane = length(pPlane);
          float deltaR = abs(dPlane - R);
          if (deltaR < 0.15f) {
            float3 uRadial = (dPlane > 1e-5f) ? (pPlane / dPlane) : sat.u;
            float3 T = cross(N, uRadial);
            float dDotT = dot(rayDir, T);
            float sinTheta = sqrt(max(0.01f, 1.0f - dDotT * dDotT));
            distPerp = deltaR * (abs(denom) / sinTheta);
            tHit = tPlane;
            hitFound = true;
          }
        }
      }

      // 3. Edge-on fallback when ray traverses bounding sphere near the plane
      if (!hitFound) {
        float t0 = clamp(-bBound, tEnter, tExit);
        float3 p0 = rayOrigin + t0 * rayDir;
        float h = dot(N, p0);
        float3 pPlane = p0 - h * N;
        float dPlane = length(pPlane);
        float deltaR = dPlane - R;
        distPerp = sqrt(h * h + deltaR * deltaR);
        tHit = t0;
        hitFound = true;
      }

      // 4. Earth occlusion and anti-aliased drawing
      if (hitFound && (tEarth < 0.0f || tHit < tEarth)) {
        // Line thickness: has solid 3D width close-up, scales with distance for anti-aliasing up to a max value
        float rTube = clamp(0.0035f * tHit, 0.008f, 0.018f);
        if (distPerp < rTube) {
          float alpha = smoothstep(rTube, rTube * 0.08f, distPerp) * 0.95f;
          finalColor = lerp(finalColor, sat.color, alpha);
        }
      }
    }
  }

  // 3. Ray - Satellite Sphere intersections (using precomputed satellite position)
  if (pc.satelliteBuffer != nullptr && pc.numSatellites > 0) {
    for (uint32_t i = 0; i < pc.numSatellites; ++i) {
      Satellite sat = pc.satelliteBuffer[i];
      float3 satPos = sat.pos;

      // Ray - Sphere intersection
      float3 oc = rayOrigin - satPos;
      float rSat = (sat.sphereRadius > 0.0f) ? sat.sphereRadius : 0.025f;
      float bSat = dot(oc, rayDir);
      float cSat = dot(oc, oc) - rSat * rSat;
      float dSat = bSat * bSat - cSat;

      if (dSat >= 0.0f) {
        float t = -bSat - sqrt(dSat);
        if (t > 0.0f && t < tNearest) {
          tNearest = t;
          finalColor = sat.color; // UNSHADED solid color, no glow
        }
      }
    }
  }

  return float4(finalColor, 1.0f);
}
