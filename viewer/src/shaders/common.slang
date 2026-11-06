#language slang 2026

module common;

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
