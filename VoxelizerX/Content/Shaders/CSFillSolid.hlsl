//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

#include "SharedConst.h"
#if	!USE_MUTEX
#include "Common\D3DX_DXGIFormatConvert.inl"
#define	pack(x)		D3DX_FLOAT4_to_R10G10B10A2_UNORM(x)
#define	unpack(x)	D3DX_R10G10B10A2_UNORM_to_FLOAT4(x)
#endif

//--------------------------------------------------------------------------------------
// Constant buffer
//--------------------------------------------------------------------------------------
cbuffer cbPerMipLevel
{
	float g_gridSize;
};

//--------------------------------------------------------------------------------------
// Textures
//--------------------------------------------------------------------------------------
Texture2DArray<uint>	g_txKBufDepth;
#if	USE_MUTEX
Texture3D<float>		g_txGrids[3];
RWTexture3D<float>		g_rwGrid;
#else
RWTexture3D<uint>		g_rwGrid;
#endif

//--------------------------------------------------------------------------------------
// Fill solid voxels
//--------------------------------------------------------------------------------------
[numthreads(4, 4, 4)]
void main(uint3 DTid : SV_DispatchThreadID)
{
#if	USE_MUTEX
	float4 data;
	data.x = g_txGrids[0][DTid];
	data.y = g_txGrids[1][DTid];
	data.z = g_txGrids[2][DTid];
	data.w = any(data.xyz);
#else
	const float4 data = unpack(g_rwGrid[DTid]);
#endif

	if (data.w <= 0.0)
	{
		const uint numLayer = g_gridSize * DEPTH_SCALE;

		bool needFill = false;
		uint depthBeg = 0xffffffff, depthEnd;

		for (uint i = 0; i < numLayer; ++i)
		{
			const uint3 vTex = { DTid.xy, i };
			depthEnd = g_txKBufDepth[vTex];
#if	USE_NORMAL
			if (depthEnd > DTid.z) break;
#else
			if (depthEnd == 0xffffffff)
			{
				needFill = false;
				break;
			}
			if (depthEnd > DTid.z) break;
			needFill = depthBeg == depthEnd - 1 ? needFill : !needFill;
#endif
			depthBeg = depthEnd;
		}

#if	USE_NORMAL
		if (depthBeg != 0xffffffff && depthEnd != 0xffffffff)
		{
#if	USE_MUTEX
			const float normBegZ = g_txGrids[2][uint3(DTid.xy, depthBeg)];
			const float normEndZ = g_txGrids[2][uint3(DTid.xy, depthEnd)];
			needFill = normBegZ < 0.0 || normEndZ > 0.0;
#else
			const float normBegZ = unpack(g_rwGrid[uint3(DTid.xy, depthBeg)]).z;
			const float normEndZ = unpack(g_rwGrid[uint3(DTid.xy, depthEnd)]).z;
			needFill = normBegZ < 0.5 || normEndZ > 0.5;
#endif
		}
#endif

		if (needFill)
		{
#if	USE_MUTEX
			g_rwGrid[DTid] = 1.0;
#else
			g_rwGrid[DTid] = pack(float4(data.xyz, 1.0));
#endif
		}
	}
}
