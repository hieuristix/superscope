#language slang 2026

module common;

public struct Satellite {
  public float theta;
  public float phi;
  public float radius;
  public float speed;
};

public struct DrawInfo {
  public column_major float3x3 transform;
  public float4 uvBounds; // left top right bottom
  public float3 color;
  public uint32_t texID;
  public uint32_t isMSDF;
};

public struct VertexOut {
  public uint32_t instID : SV_InstanceID;
  public float4 pos   : SV_Position;
  public float2 uv    : TEXCOORD0;
  public float2 uv1   : TEXCOORD1;
  public float3 color : TEXCOORD2;
};

public struct PushConstants {
  public DrawInfo* infoBuffer;
  public Satellite* satelliteBuffer;
  public float3 camPos;
  public float time;
  public float3 camForward;
  public uint32_t numSatellites;
  public float3 camRight;
  public uint32_t skyTexId;
  public float3 camUp;
  public uint32_t earthTexId;
  public float fov;
  public float aspect;
  public float earthRadius;
  public float _pad;
};
