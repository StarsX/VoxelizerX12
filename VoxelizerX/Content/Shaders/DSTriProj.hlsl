//--------------------------------------------------------------------------------------
// By XU, Tianchen
//--------------------------------------------------------------------------------------

#define NUM_CONTROL_POINTS	3

#include "DSTriProj.hlsli"

//--------------------------------------------------------------------------------------
// Struct
//--------------------------------------------------------------------------------------
// Output patch constant data.
struct HSConstDataOut
{
	float EdgeTessFactor[3]	: SV_TessFactor;		// e.g. would be [4] for a quad domain
	float InsideTessFactor	: SV_InsideTessFactor;	// e.g. would be Inside[2] for a quad domain
};

//--------------------------------------------------------------------------------------
// Perform triangle extrapolations
//--------------------------------------------------------------------------------------
[domain("tri")]
DSOut main(HSConstDataOut input,
	float3 domain : SV_DomainLocation,
	OutputPatch<DSIn, NUM_CONTROL_POINTS> patch)
{
	return DSMain(domain, patch);
}
