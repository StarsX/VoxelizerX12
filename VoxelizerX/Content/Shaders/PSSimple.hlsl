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
	const float3 lightPt = 1.0;
	const min16float3 lightDir = min16float3(normalize(lightPt));

	const min16float lightAmtMesh = saturate(dot(input.NrmMesh.xyz, lightDir));
	const min16float lightAmtCube = saturate(dot(input.NrmCube.xyz, lightDir));
	const min16float lightAmt = lerp(lightAmtMesh, lightAmtCube, NORNAL_BLEND);

	const min16float ambientMesh = lerp(0.25, 0.5, saturate(input.NrmMesh.y + 0.5));
	const min16float ambientCube = lerp(0.25, 0.5, saturate(input.NrmCube.y + 0.5));
	const min16float ambient = lerp(ambientMesh, ambientCube, NORNAL_BLEND);

	return min16float4((lightAmt + 0.75 * ambient * (input.NrmMesh.w + 0.2)).xxx, 1.0);
	//return min16float4(input.Nrm * 0.5 + 0.5, 1.0);
}
