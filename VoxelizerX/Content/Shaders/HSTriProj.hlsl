//--------------------------------------------------------------------------------------
// By XU, Tianchen
//--------------------------------------------------------------------------------------

#define NUM_CONTROL_POINTS	3

//--------------------------------------------------------------------------------------
// Structs
//--------------------------------------------------------------------------------------
// Input control point
struct VSOut
{
	float3	Pos		: POSITION;
	float3	PosLoc	: POSLOCAL;
	float3	Nrm		: NORMAL;
};

#include "HSTriProj.hlsli"

// Output patch constant data.
struct HSConstDataOut
{
	float EdgeTessFactor[3]	: SV_TessFactor;		// e.g. would be [4] for a quad domain
	float InsideTessFactor	: SV_InsideTessFactor;	// e.g. would be Inside[2] for a quad domain
};

//--------------------------------------------------------------------------------------
// Patch Constant Function
//--------------------------------------------------------------------------------------
HSConstDataOut CalcHSPatchConstants()
{
	HSConstDataOut Output;

	// Insert code to compute Output here
	Output.EdgeTessFactor[0] = Output.EdgeTessFactor[1] =
		Output.EdgeTessFactor[2] = Output.InsideTessFactor = 1.0f; // e.g. could calculate dynamic tessellation factors instead

	return Output;
}

//--------------------------------------------------------------------------------------
// Select the view with maximal projected AABB
//--------------------------------------------------------------------------------------
[domain("tri")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(NUM_CONTROL_POINTS)]
[patchconstantfunc("CalcHSPatchConstants")]
HSOut main(const InputPatch<VSOut, NUM_CONTROL_POINTS> ip, const uint i : SV_OutputControlPointID)
{
	// Calculate projected triangle sizes (equivalent to area) for 3 views
	const float3 vPrimSize = PrimSize(ip);

	// Select the view with maximal projected AABB
	const float2 vPos = Project(ip[i].Pos, vPrimSize);

	return HSMain(vPos, ip, i);
}
