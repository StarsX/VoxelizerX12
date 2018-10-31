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
	matrix	g_mWorldViewProj;
	matrix	g_mWorld;
	matrix	g_mWorldIT;
};

//--------------------------------------------------------------------------------------
// Textures
//--------------------------------------------------------------------------------------
#if	USE_MUTEX
Texture3D<min16float> g_txGrids[4];
#else
Texture3D<min16float4> g_txGrid;
#endif

static const float3x3 mPlane[6] =
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
VSOut main(const uint vID : SV_VertexID, const uint InstID : SV_InstanceID)
{
	VSOut output;

	const uint boxID = InstID / 6;
	const uint planeID = InstID % 6;

	const float2 vPos2D = float2(vID & 1, vID >> 1) * 2.0 - 1.0;
	float3 vPerBoxPos = float3(vPos2D.x, -vPos2D.y, 1.0);
	vPerBoxPos = mul(vPerBoxPos, mPlane[planeID]);

	const uint uGridSize = GRID_SIZE >> SHOW_MIP;
	const uint uSliceSize = uGridSize * uGridSize;
	const uint uPerSliceID = boxID % uSliceSize;
	const uint3 vLoc = { uPerSliceID % uGridSize, uPerSliceID / uGridSize, boxID / uSliceSize };
	float3 vPos = (vLoc * 2 + 1) / (uGridSize * 2.0);
	vPos = vPos * float3(2.0, -2.0, 2.0) + float3(-1.0, 1.0, -1.0);
	vPos += vPerBoxPos / uGridSize;
	
#if	USE_MUTEX
	min16float4 vGrid;
	vGrid.x = g_txGrids[0].mips[SHOW_MIP][vLoc];
	vGrid.y = g_txGrids[1].mips[SHOW_MIP][vLoc];
	vGrid.z = g_txGrids[2].mips[SHOW_MIP][vLoc];
	vGrid.w = g_txGrids[3].mips[SHOW_MIP][vLoc];
#else
	min16float4 vGrid = g_txGrid.mips[SHOW_MIP][vLoc];
	vGrid.xyz -= 0.5;
#endif
	
	output.Pos = mul(float4(vPos, 1.0), g_mWorldViewProj);
	output.NrmMesh = min16float4(mul(normalize(vGrid.xyz), (float3x3)g_mWorldIT), vGrid.w);
	output.NrmCube = min16float3(mul(mPlane[planeID][2], (float3x3)g_mWorldIT));

	output.Pos.w = vGrid.w > 0.0 ? output.Pos.w : 0.0;

	return output;
}
