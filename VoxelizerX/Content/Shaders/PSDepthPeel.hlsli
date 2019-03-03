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
RWTexture2DArray<uint>	g_rwKBufDepth : register(U_DEPTH);

//--------------------------------------------------------------------------------------
// Depth peeling
//--------------------------------------------------------------------------------------
void DepthPeel(uint depth, const uint2 loc, const uint numLayer)
{
	uint depthPrev;

	//[allow_uav_condition]
	for (uint i = 0; i < numLayer; ++i)
	{
		const uint3 tex = { loc, i };
		InterlockedMin(g_rwKBufDepth[tex], depth, depthPrev);

#if	USE_NORMAL
		if (depthPrev == 0xffffffff) return;

		const uint depthMax = max(depth, depthPrev);
		const uint depthMin = min(depth, depthPrev);
		if (depthMax - depthMin <= 1) return;
#else
		if (depthPrev == depth || depthPrev == 0xffffffff) return;
		const uint depthMax = max(depth, depthPrev);
#endif

		depth = depthMax;
	}
}
