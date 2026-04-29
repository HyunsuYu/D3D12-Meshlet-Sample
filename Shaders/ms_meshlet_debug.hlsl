cbuffer cbPerObject : register(b0)
{
    float4x4 gWorldViewProj;
};

struct Vertex {
    float3 Pos;
    float4 Color;
    float pad;
};
struct Meshlet {
    uint VertCount;
    uint VertOffset;
    uint PrimCount;
    uint PrimOffset;
};
struct MS_OUTPUT {
    float4 PositionCS : SV_POSITION;

    // For GCVT Ver Meshlet
    nointerpolation float3 Color : COLOR;
};

uint LoadByte(ByteAddressBuffer buffer, uint byteOffset)
{
    // 1. 4의 배수 위치로 내림 (예: 3, 4, 5, 6 -> 모두 4가 됨)
    uint alignedOffset = byteOffset & ~3;

    // 2. 해당 위치에서 4바이트(uint)를 통째로 읽음
    uint packed = buffer.Load(alignedOffset);

    // 3. 내가 원하는 바이트가 packed 안에서 몇 번째인지 계산 (0, 8, 16, 24 비트 시프트)
    uint shift = (byteOffset & 3) * 8;

    // 4. 비트 이동 후 마스킹(0xFF)하여 원하는 1바이트만 추출
    return (packed >> shift) & 0xFF;
}

float3 GetMeshletColor(uint meshletIndex)
{
    uint h = meshletIndex * 0x9E3779B9u;
    h ^= (h >> 16); h *= 0x85EBCA6Bu; h ^= (h >> 13); h *= 0xC2B2AE35u; h ^= (h >> 16);
    return float3(float(h & 0xFF), float((h >> 8) & 0xFF), float((h >> 16) & 0xFF)) / 255.0;
}

StructuredBuffer<Vertex>  gVertices     : register(t0);
StructuredBuffer<Meshlet> gMeshlets     : register(t1);
StructuredBuffer<uint>    gMeshletVerts : register(t2);
ByteAddressBuffer         gMeshletPrims : register(t3);

[OutputTopology("triangle")]
[NumThreads(128, 1, 1)]
void ms_main(
    uint gtid : SV_GroupThreadID,
    uint gid : SV_GroupID,
    out indices uint3 tris[128],
    out vertices MS_OUTPUT verts[64]
)
{
    Meshlet m = gMeshlets[gid];
    SetMeshOutputCounts(m.VertCount, m.PrimCount);

    if (gtid < m.VertCount)
    {
        uint vertexIndex = gMeshletVerts[m.VertOffset + gtid];
        float3 pos = gVertices[vertexIndex].Pos;

        verts[gtid].PositionCS = mul(float4(pos, 1.0f), gWorldViewProj);
        verts[gtid].Color = GetMeshletColor(gid);
    }

    if (gtid < m.PrimCount)
    {
        uint primOffsetBytes = m.PrimOffset;
        uint currentTriOffset = primOffsetBytes + (gtid * 3);

        uint i0 = LoadByte(gMeshletPrims, currentTriOffset + 0);
        uint i1 = LoadByte(gMeshletPrims, currentTriOffset + 1);
        uint i2 = LoadByte(gMeshletPrims, currentTriOffset + 2);

        i0 = min(i0, m.VertCount - 1);
        i1 = min(i1, m.VertCount - 1);
        i2 = min(i2, m.VertCount - 1);

        tris[gtid] = uint3(i0, i1, i2);
    }
}

float4 ps_main(MS_OUTPUT input) : SV_TARGET
{
    return float4(input.Color, 1.0f);
}