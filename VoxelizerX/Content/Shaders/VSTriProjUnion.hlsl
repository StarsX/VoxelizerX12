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
	float4	Pos		: SV_POSITION;
	float3	PosLoc	: POSLOCAL;
	float3	Nrm		: NORMAL;
	float3	TexLoc	: TEXLOCATION;
};

//--------------------------------------------------------------------------------------
// Constant buffer
//--------------------------------------------------------------------------------------
cbuffer cbPerObject
{
	float3	g_center	: packoffset(c0);
	float	g_radius	: packoffset(c0.w);
};

//--------------------------------------------------------------------------------------
// Position normalization and projection to 3 views
//--------------------------------------------------------------------------------------
VSOut main(VSIn input, uint viewID : SV_InstanceID)
{
	VSOut output;

	// Normalize
	const float3 pos = (input.Pos - g_center) / g_radius;

	// Select the view
	output.Pos.xy = viewID == 0 ? pos.xy : (viewID == 1 ? pos.yz : pos.zx);
	output.Pos.zw = float2(0.5, 1.0);

	// Other attributes
	output.PosLoc = input.Pos;
	output.Nrm = input.Nrm;
	output.TexLoc = pos * 0.5 + 0.5;
	output.TexLoc.y = 1.0 - output.TexLoc.y;

	return output;
}
