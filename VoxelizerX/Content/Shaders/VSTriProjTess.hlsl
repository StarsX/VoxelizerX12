//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
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
	float3	g_center	: packoffset(c0);
	float	g_radius	: packoffset(c0.w);
};

//--------------------------------------------------------------------------------------
// Position normalization
//--------------------------------------------------------------------------------------
VSOut main(VSIn input)
{
	VSOut output;

	output.Pos = (input.Pos - g_center) / g_radius;
	output.PosLoc = input.Pos;
	output.Nrm = input.Nrm;

	return output;
}
