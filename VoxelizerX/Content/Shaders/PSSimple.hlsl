//--------------------------------------------------------------------------------------
// By XU, Tianchen
//--------------------------------------------------------------------------------------

#define	NORNAL_BLEND	0.75

//--------------------------------------------------------------------------------------
// Struct
//--------------------------------------------------------------------------------------
struct PSIn
{
	float4	Pos			: SV_POSITION;
	min16float4	NrmMesh	: MESHNORMAL;
	min16float3	NrmCube	: CUBENORMAL;
};

//--------------------------------------------------------------------------------------
// Perform simple lighting
//--------------------------------------------------------------------------------------
min16float4 main(const PSIn input) : SV_TARGET
{
	const float3 vLightPt = 1.0;
	const min16float3 vLightDir = min16float3(normalize(vLightPt));

	const min16float fLightAmtMesh = saturate(dot(input.NrmMesh.xyz, vLightDir));
	const min16float fLightAmtCube = saturate(dot(input.NrmCube.xyz, vLightDir));
	const min16float fLightAmt = lerp(fLightAmtMesh, fLightAmtCube, NORNAL_BLEND);

	const min16float fAmbientMesh = lerp(0.25, 0.5, saturate(input.NrmMesh.y + 0.5));
	const min16float fAmbientCube = lerp(0.25, 0.5, saturate(input.NrmCube.y + 0.5));
	const min16float fAmbient = lerp(fAmbientMesh, fAmbientCube, NORNAL_BLEND);

	return min16float4((fLightAmt + 0.75 * fAmbient * (input.NrmMesh.w + 0.2)).xxx, 1.0);
	//return min16float4(input.Nrm * 0.5 + 0.5, 1.0);
}
