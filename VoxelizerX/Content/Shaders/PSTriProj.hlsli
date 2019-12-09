//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

#include "SharedConst.h"
#if	USE_MUTEX
#include "CSMutex.hlsli"
#else
#include "Common\D3DX_DXGIFormatConvert.inl"
#endif

//--------------------------------------------------------------------------------------
// Struct
//--------------------------------------------------------------------------------------
struct PSIn
{
	float4	Pos		: SV_POSITION;
	float3	PosLoc	: POSLOCAL;
	float3	Nrm		: NORMAL;
	float3	TexLoc	: TEXLOCATION;
#ifdef _CONSERVATIVE_
	float4	Bound	: AABB;
#endif
};

//--------------------------------------------------------------------------------------
// Constant buffer
//--------------------------------------------------------------------------------------
cbuffer cbPerMipLevel
{
	float g_gridSize;
};

//--------------------------------------------------------------------------------------
// Unordered access textures
//--------------------------------------------------------------------------------------
#if	USE_MUTEX
globallycoherent
RWTexture3D<float>	g_rwGrids[4]	: register (u0);
#else
RWTexture3D<uint>	g_rwGrid		: register (u0);
#endif

//--------------------------------------------------------------------------------------
// Surface voxelization, writing to grid
//--------------------------------------------------------------------------------------
void PSTriProj(PSIn input, uint3 loc)
{
	const float3 normal = normalize(input.Nrm);
#if	!USE_MUTEX
	const float4 data = { normal * 0.5 + 0.5, 1.0 };
	uint packedData = D3DX_FLOAT4_to_R10G10B10A2_UNORM(data);
#endif
	
#ifdef _CONSERVATIVE_
	const float4 bound = input.Bound * g_gridSize;
	const bool needWrite = input.Pos.x + 1.0 > bound.x && input.Pos.y + 1.0 > bound.y &&
		input.Pos.x < bound.z + 1.0 && input.Pos.y < bound.w + 1.0;
#endif

#if	USE_MUTEX

	mutexLock(loc);
	// Critical section
#ifdef _CONSERVATIVE_
	if (needWrite)
#endif
	{
		g_RWGrids[0][loc] += normal.x;
		g_RWGrids[1][loc] += normal.y;
		g_RWGrids[2][loc] += normal.z;
		g_RWGrids[3][loc] = 1.0;
	}
	mutexUnlock(loc);

#else

#ifdef _CONSERVATIVE_
	if (needWrite)
#endif
		InterlockedMax(g_rwGrid[loc], packedData, packedData);

#endif
}
