//--------------------------------------------------------------------------------------
// By XU, Tianchen
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
	float g_fGridSize;
};

//--------------------------------------------------------------------------------------
// Unordered access textures
//--------------------------------------------------------------------------------------
#if	USE_MUTEX
globallycoherent
RWTexture3D<min16float>	g_RWGrids[4]	: register (u0);
#else
RWTexture3D<uint>		g_RWGrid		: register (u0);
#endif

//--------------------------------------------------------------------------------------
// Surface voxelization, writing to grid
//--------------------------------------------------------------------------------------
void PSTriProj(const PSIn input, const uint3 vLoc)
{
#if	USE_MUTEX
	const min16float3 vNorm = min16float3(normalize(input.Nrm));
#else
	const float3 vNorm = normalize(input.Nrm);
	const float4 vData = { vNorm * 0.5 + 0.5, 1.0 };
	uint uData = D3DX_FLOAT4_to_R10G10B10A2_UNORM(vData);

#endif
	
#ifdef _CONSERVATIVE_
	const float4 vBound = input.Bound * g_fGridSize;
	const bool bWrite = input.Pos.x + 1.0 > vBound.x && input.Pos.y + 1.0 > vBound.y &&
		input.Pos.x < vBound.z + 1.0 && input.Pos.y < vBound.w + 1.0;
#endif

#if	USE_MUTEX

	mutexLock(vLoc);
	// Critical section
#ifdef _CONSERVATIVE_
	if (bWrite)
#endif
	{
		g_RWGrids[0][vLoc] += vNorm.x;
		g_RWGrids[1][vLoc] += vNorm.y;
		g_RWGrids[2][vLoc] += vNorm.z;
		g_RWGrids[3][vLoc] = 1.0;
	}
	mutexUnlock(vLoc);

#else

#ifdef _CONSERVATIVE_
	if (bWrite)
#endif
		InterlockedMax(g_RWGrid[vLoc], uData, uData);

#endif
}
