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
	const float3 vEdge1 = ip[1].Pos - ip[0].Pos;
	const float3 vEdge2 = ip[2].Pos - ip[1].Pos;

	// Calculate projected triangle sizes (equivalent to area) for 3 views
	const float fSizeXY = abs(determinant(float2x2(vEdge1.xy, vEdge2.xy)));
	const float fSizeYZ = abs(determinant(float2x2(vEdge1.yz, vEdge2.yz)));
	const float fSizeZX = abs(determinant(float2x2(vEdge1.zx, vEdge2.zx)));

	return float3(fSizeXY, fSizeYZ, fSizeZX);
}

//--------------------------------------------------------------------------------------
// Project to 3-views
//--------------------------------------------------------------------------------------
float2 Project(const float3 vPos, const float3 vPrimSize)
{
	const float fSizeXY = vPrimSize.x;
	const float fSizeYZ = vPrimSize.y;
	const float fSizeZX = vPrimSize.z;

	return fSizeXY > fSizeYZ ?
		(fSizeXY > fSizeZX ? vPos.xy : vPos.zx) :
		(fSizeYZ > fSizeZX ? vPos.yz : vPos.zx);
}

//--------------------------------------------------------------------------------------
// Select the view with maximal projected AABB
//--------------------------------------------------------------------------------------
HSOut HSMain(const float2 vPos, const InputPatch<VSOut, NUM_CONTROL_POINTS> ip, const uint i)
{
	HSOut output;

	// Select the view with maximal projected AABB
	output.Pos = vPos;

	// Other attributes
	output.PosLoc = ip[i].PosLoc;
	output.Nrm = ip[i].Nrm;

	// Texture 3D space
	output.TexLoc = ip[i].Pos * 0.5 + 0.5;
	output.TexLoc.y = 1.0 - output.TexLoc.y;

	return output;
}
