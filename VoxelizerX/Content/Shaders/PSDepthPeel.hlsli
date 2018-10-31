// --------------------------------------------------------------------------------------
// By XU, Tianchen
//--------------------------------------------------------------------------------------

#ifndef U_DEPTH
#if	USE_MUTEX
#define	U_DEPTH	u5
#else
#define	U_DEPTH	u1
#endif
#endif

//--------------------------------------------------------------------------------------
// Unordered access texture
//--------------------------------------------------------------------------------------
RWTexture2DArray<uint>	g_RWKBufDepth : register(U_DEPTH);

//--------------------------------------------------------------------------------------
// Depth peeling
//--------------------------------------------------------------------------------------
void DepthPeel(uint uDepth, const uint2 vLoc, const uint uNumLayer)
{
	uint uDepthPrev;

	//[allow_uav_condition]
	for (uint i = 0; i < uNumLayer; ++i)
	{
		const uint3 vTex = { vLoc, i };
		InterlockedMin(g_RWKBufDepth[vTex], uDepth, uDepthPrev);

#if	USE_NORMAL
		if (uDepthPrev == 0xffffffff) return;

		const uint uDepthMax = max(uDepth, uDepthPrev);
		const uint uDepthMin = min(uDepth, uDepthPrev);
		if (uDepthMax - uDepthMin <= 1) return;
#else
		if (uDepthPrev == uDepth || uDepthPrev == 0xffffffff) return;
		const uint uDepthMax = max(uDepth, uDepthPrev);
#endif

		uDepth = uDepthMax;
	}
}
