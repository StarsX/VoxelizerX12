//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

#include "PSTriProj.hlsli"

//--------------------------------------------------------------------------------------
// Surface voxelization and depth peeling for solid voxelization
//--------------------------------------------------------------------------------------
void main(PSIn input)
{
	const uint3 loc = input.TexLoc * g_gridSize;
	
	PSTriProj(input, loc);
}
