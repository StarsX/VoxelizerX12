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
	float3	g_vCenter	: packoffset(c0);
	float	g_fRadius	: packoffset(c0.w);
};

//--------------------------------------------------------------------------------------
// Position normalization and projection to 3 views
//--------------------------------------------------------------------------------------
VSOut main(const VSIn input, const uint viewID : SV_InstanceID)
{
	VSOut output;

	// Normalize
	const float3 vPos = (input.Pos - g_vCenter) / g_fRadius;

	// Select the view
	output.Pos.xy = viewID == 0 ? vPos.xy : (viewID == 1 ? vPos.yz : vPos.zx);
	output.Pos.zw = float2(0.5, 1.0);

	// Other attributes
	output.PosLoc = input.Pos;
	output.Nrm = input.Nrm;
	output.TexLoc = vPos * 0.5 + 0.5;
	output.TexLoc.y = 1.0 - output.TexLoc.y;

	return output;
}
