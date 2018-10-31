//--------------------------------------------------------------------------------------
// By XU, Tianchen
//--------------------------------------------------------------------------------------

#include "CHDataSize.hlsli"

#define	main	VertShader
#include "VSTriProjTess.hlsl"
#undef main

#define NUM_CONTROL_POINTS	3

#include "HSTriProj.hlsli"
#include "DSTriProj.hlsli"

static const uint g_uStrideVB = SIZE_OF_FLOAT3 * 2;

//--------------------------------------------------------------------------------------
// Buffers
//--------------------------------------------------------------------------------------
ByteAddressBuffer g_roIndices;
ByteAddressBuffer g_roVertices;

//--------------------------------------------------------------------------------------
// Load IA
//--------------------------------------------------------------------------------------
VSIn LoadVSIn(const uint uIdx)
{
	VSIn result;
	const uint uVid = g_roIndices.Load(SIZE_OF_UINT * uIdx);
	const uint uBase = uVid * g_uStrideVB;

	result.Pos = asfloat(g_roVertices.Load3(uBase));
	result.Nrm = asfloat(g_roVertices.Load3(uBase + SIZE_OF_FLOAT3));

	return result;
}

//--------------------------------------------------------------------------------------
// Emulate the behaviors of VS, HS, and DS
//--------------------------------------------------------------------------------------
DSOut main(const uint vID : SV_VertexID, const uint vPrimID : SV_InstanceID)
{
	DSOut output;

	// Call vertex shaders
	VSOut ip[] =
	{
		VertShader(LoadVSIn(vPrimID * NUM_CONTROL_POINTS)),
		VertShader(LoadVSIn(vPrimID * NUM_CONTROL_POINTS + 1)),
		VertShader(LoadVSIn(vPrimID * NUM_CONTROL_POINTS + 2))
	};

	// Calculate projected triangle sizes (equivalent to area) for 3 views
	const float3 vPrimSize = PrimSize(ip);

	// Select the view with maximal projected AABB
	float2 v[] =
	{
		Project(ip[0].Pos, vPrimSize),
		Project(ip[1].Pos, vPrimSize),
		Project(ip[2].Pos, vPrimSize),
	};
	
	// Call hull shaders
	HSOut patch[] =
	{
		HSMain(v[0], ip, 0),
		HSMain(v[1], ip, 1),
		HSMain(v[2], ip, 2)
	};
	
	// Call domain shader
	const float3 domain = { vID == 0, vID == 1, vID == 2 };
	output = DSMain(domain, patch);

	return output;
}
