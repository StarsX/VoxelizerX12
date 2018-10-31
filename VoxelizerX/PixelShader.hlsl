#define NUM_TEXTURES	8

struct PSInput
{
	float4 position : SV_POSITION;
	float2 uv : TEXCOORD;
	float4 color : COLOR;
};

Texture2D g_textures[NUM_TEXTURES] : register(t0);
SamplerState g_sampler : register(s0);

float4 main(PSInput input) : SV_TARGET
{
	float4 color = 0.0;

	[unroll]
	for (uint i = 0; i < NUM_TEXTURES; i++)
		color += g_textures[i].Sample(g_sampler, input.uv);
	
	return color / NUM_TEXTURES * input.color;
}
