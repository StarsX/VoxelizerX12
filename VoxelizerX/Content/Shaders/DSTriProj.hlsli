//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

#define CONSERVATION_AMT	1.0 / 3.0	// Measured by pixels, e.g. 1.0 / 3.0 means 1/3 pixel size

//--------------------------------------------------------------------------------------
// Structs
//--------------------------------------------------------------------------------------
struct DSIn
{
	float2	Pos		: POSITION;
	float3	PosLoc	: POSLOCAL;
	float3	Nrm		: NORMAL;
	float3	TexLoc	: TEXLOCATION;
};

struct DSOut
{
	float4	Pos		: SV_POSITION;
	float3	PosLoc	: POSLOCAL;
	float3	Nrm		: NORMAL;
	float3	TexLoc	: TEXLOCATION;
	float4	Bound	: AABB;
};

//--------------------------------------------------------------------------------------
// Constant buffer
//--------------------------------------------------------------------------------------
cbuffer cbPerMipLevel
{
	float g_gridSize;
};

//--------------------------------------------------------------------------------------
// Move the vertex by the pixel bias.
//--------------------------------------------------------------------------------------
float2 Scale(float2 pv, float2 cv, float2 nv, float pixelBias = 0.5)
{
	float3 plane0 = cross(float3(cv - pv, 0.0), float3(pv, 1.0));
	float3 plane1 = cross(float3(nv - cv, 0.0), float3(cv, 1.0));

	plane0.z -= dot(pixelBias, abs(plane0.xy));
	plane1.z -= dot(pixelBias, abs(plane1.xy));

	const float3 result = cross(plane0, plane1);

	return result.xy / result.z;
}

//--------------------------------------------------------------------------------------
// Perform triangle extrapolations
//--------------------------------------------------------------------------------------
DSOut DSMain(float3 domain, OutputPatch<DSIn, NUM_CONTROL_POINTS> patch)
{
	DSOut output;

	const float gridHalfSize = g_gridSize * 0.5;
	const float bias = 0.1 / gridHalfSize;
#define V(i) patch[i].Pos
	[unroll]
	for (uint i = 0; domain[i] <= 0.0 && i < 3; ++i);
	const float2 scaledVert = Scale(V(i < 1 ? 2 : i - 1), V(i), V(i > 1 ? 0 : i + 1), bias);
#undef V

	// Distance to centroid in rasterizer space
	//const float2 vertex = patch[i].Pos;
	const float2 vertex = patch[0].Pos * domain.x + patch[1].Pos * domain.y + patch[2].Pos * domain.z;
	const float2 centroidPos = (patch[0].Pos + patch[1].Pos + patch[2].Pos) / 3.0;
	const float dist = distance(vertex, centroidPos);

	// Change domain location with offset for extrapolation
	const float3 onePixelOffset = (domain - 1.0 / 3.0) / dist;
	//domain += distance(scaledVert, vertex) * onePixelOffset;
	domain += CONSERVATION_AMT / gridHalfSize * onePixelOffset;

	// Extrapolations
	output.Pos.xy = patch[0].Pos * domain.x + patch[1].Pos * domain.y + patch[2].Pos * domain.z;
	output.PosLoc = patch[0].PosLoc * domain.x + patch[1].PosLoc * domain.y + patch[2].PosLoc * domain.z;
	output.Nrm = patch[0].Nrm * domain.x + patch[1].Nrm * domain.y + patch[2].Nrm * domain.z;
	output.TexLoc = patch[0].TexLoc * domain.x + patch[1].TexLoc * domain.y + patch[2].TexLoc * domain.z;
	output.Pos.zw = float2(0.5, 1.0);

	// Calculate projected AABB
	output.Bound.xy = min(min(patch[0].Pos, patch[1].Pos), patch[2].Pos);
	output.Bound.zw = max(max(patch[0].Pos, patch[1].Pos), patch[2].Pos);
	output.Bound = output.Bound * 0.5 + 0.5;
	output.Bound.yw = 1.0 - output.Bound.wy;

	return output;
}
