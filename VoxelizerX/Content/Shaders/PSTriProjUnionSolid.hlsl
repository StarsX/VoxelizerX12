//--------------------------------------------------------------------------------------
// By XU, Tianchen
//--------------------------------------------------------------------------------------

#include "PSTriProj.hlsli"
#include "PSDepthPeel.hlsli"

//--------------------------------------------------------------------------------------
// Surface voxelization and depth peeling for solid voxelization
//--------------------------------------------------------------------------------------
void main(PSIn input)
{
	const uint3 loc = input.TexLoc * g_gridSize;
	
	PSTriProj(input, loc);
	DepthPeel(loc.z, loc.xy, g_gridSize * DEPTH_SCALE);
}
