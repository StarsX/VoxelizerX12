//--------------------------------------------------------------------------------------
// By XU, Tianchen
//--------------------------------------------------------------------------------------

#include "SharedConst.h"

//--------------------------------------------------------------------------------------
// Struct
//--------------------------------------------------------------------------------------
struct VSOut
{
	float4		Pos		: SV_POSITION;
	min16float4	NrmMesh	: MESHNORMAL;
	min16float3	NrmCube	: CUBENORMAL;
};

//--------------------------------------------------------------------------------------
// Constant buffer
//--------------------------------------------------------------------------------------
cbuffer cbMatrices
{
	matrix	g_worldViewProj;
	matrix	g_world;
	matrix	g_worldIT;
};

//--------------------------------------------------------------------------------------
// Textures
//--------------------------------------------------------------------------------------
#if	USE_MUTEX
Texture3D<float>	g_txGrids[4];
#else
Texture3D			g_txGrid;
#endif

static const float3x3 plane[6] =
{
	// back plane
	float3x3(-1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f),
	// left plane
	float3x3(0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f),
	// front plane
	float3x3(1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f),
	// right plane
	float3x3(0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f),
	// top plane
	float3x3(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f),
	// bottom plane
	float3x3(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f)
};

//--------------------------------------------------------------------------------------
// Generate boxes
//--------------------------------------------------------------------------------------
VSOut main(uint vID : SV_VertexID, uint instID : SV_InstanceID)
{
	VSOut output;

	const uint boxID = instID / 6;
	const uint planeID = instID % 6;

	const float2 pos2D = float2(vID & 1, vID >> 1) * 2.0 - 1.0;
	float3 perBoxPos = float3(pos2D.x, -pos2D.y, 1.0);
	perBoxPos = mul(perBoxPos, plane[planeID]);

	const uint gridSize = GRID_SIZE >> SHOW_MIP;
	const uint sliceSize = gridSize * gridSize;
	const uint perSliceID = boxID % sliceSize;
	const uint3 loc = { perSliceID % gridSize, perSliceID / gridSize, boxID / sliceSize };
	float3 pos = (loc * 2 + 1) / (gridSize * 2.0);
	pos = pos * float3(2.0, -2.0, 2.0) + float3(-1.0, 1.0, -1.0);
	pos += perBoxPos / gridSize;
	
#if	USE_MUTEX
	min16float4 grid;
	grid.x = g_txGrids[0].mips[SHOW_MIP][vLoc];
	grid.y = g_txGrids[1].mips[SHOW_MIP][vLoc];
	grid.z = g_txGrids[2].mips[SHOW_MIP][vLoc];
	grid.w = g_txGrids[3].mips[SHOW_MIP][vLoc];
#else
	min16float4 grid = min16float4(g_txGrid.mips[SHOW_MIP][loc]);
	grid.xyz -= 0.5;
#endif
	
	output.Pos = mul(float4(pos, 1.0), g_worldViewProj);
	output.NrmMesh = min16float4(mul(normalize(grid.xyz), (float3x3)g_worldIT), grid.w);
	output.NrmCube = min16float3(mul(plane[planeID][2], (float3x3)g_worldIT));

	output.Pos.w = grid.w > 0.0 ? output.Pos.w : 0.0;

	return output;
}
