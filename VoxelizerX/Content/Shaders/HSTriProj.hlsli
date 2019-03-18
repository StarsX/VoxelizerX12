//--------------------------------------------------------------------------------------
// By XU, Tianchen
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Struct
//--------------------------------------------------------------------------------------
// Output control point
struct HSOut
{
	float2	Pos		: POSITION;
	float3	PosLoc	: POSLOCAL;
	float3	Nrm		: NORMAL;
	float3	TexLoc	: TEXLOCATION;
};

//--------------------------------------------------------------------------------------
// Calculate projected triangle sizes (equivalent to area) for 3 views
//--------------------------------------------------------------------------------------
float3 PrimSize(VSOut ip[NUM_CONTROL_POINTS])
{
	// Calculate projected edges of 3-views respectively
	const float3 edge1 = ip[1].Pos - ip[0].Pos;
	const float3 edge2 = ip[2].Pos - ip[1].Pos;

	// Calculate projected triangle sizes (equivalent to area) for 3 views
	const float sizeXY = abs(determinant(float2x2(edge1.xy, edge2.xy)));
	const float sizeYZ = abs(determinant(float2x2(edge1.yz, edge2.yz)));
	const float sizeZX = abs(determinant(float2x2(edge1.zx, edge2.zx)));

	return float3(sizeXY, sizeYZ, sizeZX);
}

//--------------------------------------------------------------------------------------
// Project to 3-views
//--------------------------------------------------------------------------------------
float2 Project(float3 pos, float3 primSize)
{
	const float sizeXY = primSize.x;
	const float sizeYZ = primSize.y;
	const float sizeZX = primSize.z;

	return sizeXY > sizeYZ ?
		(sizeXY > sizeZX ? pos.xy : pos.zx) :
		(sizeYZ > sizeZX ? pos.yz : pos.zx);
}

//--------------------------------------------------------------------------------------
// Select the view with maximal projected AABB
//--------------------------------------------------------------------------------------
HSOut HSMain(float2 pos, VSOut ip[NUM_CONTROL_POINTS], uint i)
{
	HSOut output;

	// Select the view with maximal projected AABB
	output.Pos = pos;

	// Other attributes
	output.PosLoc = ip[i].PosLoc;
	output.Nrm = ip[i].Nrm;

	// Texture 3D space
	output.TexLoc = ip[i].Pos * 0.5 + 0.5;
	output.TexLoc.y = 1.0 - output.TexLoc.y;

	return output;
}
