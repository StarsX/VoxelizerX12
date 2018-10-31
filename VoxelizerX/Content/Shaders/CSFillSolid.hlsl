//--------------------------------------------------------------------------------------
// By XU, Tianchen
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
	float g_fGridSize;
};

//--------------------------------------------------------------------------------------
// Textures
//--------------------------------------------------------------------------------------
Texture2DArray<uint>	g_txKBufDepth;
#if	USE_MUTEX
Texture3D<min16float>	g_txGrids[3];
RWTexture3D<min16float>	g_RWGrid;
#else
RWTexture3D<uint>		g_RWGrid;
#endif

//--------------------------------------------------------------------------------------
// Fill solid voxels
//--------------------------------------------------------------------------------------
[numthreads(32, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
#if	USE_MUTEX
	min16float4 vData;
	vData.x = g_txGrids[0][DTid];
	vData.y = g_txGrids[1][DTid];
	vData.z = g_txGrids[2][DTid];
	vData.w = any(vData.xyz);
#else
	const float4 vData = unpack(g_RWGrid[DTid]);
#endif

	if (vData.w <= 0.0)
	{
		const uint uNumLayer = g_fGridSize * DEPTH_SCALE;

		bool bFill = false;
		uint uDepthBeg = 0xffffffff, uDepthEnd;

		for (uint i = 0; i < uNumLayer; ++i)
		{
			const uint3 vTex = { DTid.xy, i };
			uDepthEnd = g_txKBufDepth[vTex];
#if	USE_NORMAL
			if (uDepthEnd > DTid.z) break;
#else
			if (uDepthEnd == 0xffffffff)
			{
				bFill = false;
				break;
			}
			if (uDepthEnd > DTid.z) break;
			bFill = uDepthBeg == uDepthEnd - 1 ? bFill : !bFill;
#endif
			uDepthBeg = uDepthEnd;
		}

#if	USE_NORMAL
		if (uDepthBeg != 0xffffffff && uDepthEnd != 0xffffffff)
		{
#if	USE_MUTEX
			const min16float vNormBegZ = g_txGrids[2][uint3(DTid.xy, uDepthBeg)];
			const min16float vNormEndZ = g_txGrids[2][uint3(DTid.xy, uDepthEnd)];
			bFill = vNormBegZ < 0.0 || vNormEndZ > 0.0;
#else
			const float vNormBegZ = unpack(g_RWGrid[uint3(DTid.xy, uDepthBeg)]).z;
			const float vNormEndZ = unpack(g_RWGrid[uint3(DTid.xy, uDepthEnd)]).z;
			bFill = vNormBegZ < 0.5 || vNormEndZ > 0.5;
#endif
		}
#endif

		if (bFill)
		{
#if	USE_MUTEX
			g_RWGrid[DTid] = 1.0;
#else
			g_RWGrid[DTid] = pack(float4(vData.xyz, 1.0));
#endif
		}
	}
}
