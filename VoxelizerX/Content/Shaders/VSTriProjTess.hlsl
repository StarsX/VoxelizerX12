//--------------------------------------------------------------------------------------
// By XU, Tianchen
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Structs
//--------------------------------------------------------------------------------------
struct VSIn
{
	float3	Pos		: POSITION;
	float3	Nrm		: NORMAL;
};

struct VSOut
{
	float3	Pos		: POSITION;
	float3	PosLoc	: POSLOCAL;
	float3	Nrm		: NORMAL;
};

//--------------------------------------------------------------------------------------
// Constant buffer
//--------------------------------------------------------------------------------------
cbuffer cbPerObject
{
	float3	g_vCenter	: packoffset(c0);
	float	g_fRadius	: packoffset(c0.w);
};

//--------------------------------------------------------------------------------------
// Position normalization
//--------------------------------------------------------------------------------------
VSOut main(const VSIn input)
{
	VSOut output;

	output.Pos = (input.Pos - g_vCenter) / g_fRadius;
	output.PosLoc = input.Pos;
	output.Nrm = input.Nrm;

	return output;
}
