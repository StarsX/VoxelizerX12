//--------------------------------------------------------------------------------------
// By Stars XU Tianchen
//--------------------------------------------------------------------------------------

using namespace std;

namespace XUSG
{
	DescriptorView SamplerPointWrap()
	{
		const auto sampler = make_shared<Descriptor>();
		sampler->ResourceType = Descriptor::SAMPLER;
		sampler->Samp.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		sampler->Samp.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler->Samp.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler->Samp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler->Samp.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		sampler->Samp.MinLOD = 0.0f;
		sampler->Samp.MaxLOD = D3D12_FLOAT32_MAX;

		return sampler;
	}

	DescriptorView SamplerPointClamp()
	{
		const auto sampler = make_shared<Descriptor>();
		sampler->ResourceType = Descriptor::SAMPLER;
		sampler->Samp.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		sampler->Samp.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		sampler->Samp.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		sampler->Samp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		sampler->Samp.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		sampler->Samp.MinLOD = 0.0f;
		sampler->Samp.MaxLOD = D3D12_FLOAT32_MAX;

		return sampler;
	}

	DescriptorView SamplerPointBorder()
	{
		const auto sampler = make_shared<Descriptor>();
		sampler->ResourceType = Descriptor::SAMPLER;
		sampler->Samp.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		sampler->Samp.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler->Samp.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler->Samp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler->Samp.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		sampler->Samp.MinLOD = 0.0f;
		sampler->Samp.MaxLOD = D3D12_FLOAT32_MAX;

		return sampler;
	}

	DescriptorView SamplerPointLessEqual()
	{
		const auto sampler = make_shared<Descriptor>();
		sampler->ResourceType = Descriptor::SAMPLER;
		sampler->Samp.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		sampler->Samp.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler->Samp.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler->Samp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler->Samp.BorderColor[0] = 1.0f;
		sampler->Samp.BorderColor[1] = 1.0f;
		sampler->Samp.BorderColor[2] = 1.0f;
		sampler->Samp.BorderColor[3] = 1.0f;
		sampler->Samp.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		sampler->Samp.MinLOD = 0.0f;
		sampler->Samp.MaxLOD = D3D12_FLOAT32_MAX;

		return sampler;
	}

	DescriptorView SamplerLinearWrap()
	{
		const auto sampler = make_shared<Descriptor>();
		sampler->ResourceType = Descriptor::SAMPLER;
		sampler->Samp.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		sampler->Samp.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler->Samp.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler->Samp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler->Samp.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		sampler->Samp.MinLOD = 0.0f;
		sampler->Samp.MaxLOD = D3D12_FLOAT32_MAX;

		return sampler;
	}

	DescriptorView SamplerLinearClamp()
	{
		const auto sampler = make_shared<Descriptor>();
		sampler->ResourceType = Descriptor::SAMPLER;
		sampler->Samp.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		sampler->Samp.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		sampler->Samp.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		sampler->Samp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		sampler->Samp.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		sampler->Samp.MinLOD = 0.0f;
		sampler->Samp.MaxLOD = D3D12_FLOAT32_MAX;

		return sampler;
	}

	DescriptorView SamplerLinearBorder()
	{
		const auto sampler = make_shared<Descriptor>();
		sampler->ResourceType = Descriptor::SAMPLER;
		sampler->Samp.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		sampler->Samp.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler->Samp.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler->Samp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler->Samp.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		sampler->Samp.MinLOD = 0.0f;
		sampler->Samp.MaxLOD = D3D12_FLOAT32_MAX;

		return sampler;
	}

	DescriptorView SamplerLinearLessEqual()
	{
		const auto sampler = make_shared<Descriptor>();
		sampler->ResourceType = Descriptor::SAMPLER;
		sampler->Samp.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		sampler->Samp.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler->Samp.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler->Samp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler->Samp.BorderColor[0] = 1.0f;
		sampler->Samp.BorderColor[1] = 1.0f;
		sampler->Samp.BorderColor[2] = 1.0f;
		sampler->Samp.BorderColor[3] = 1.0f;
		sampler->Samp.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		sampler->Samp.MinLOD = 0.0f;
		sampler->Samp.MaxLOD = D3D12_FLOAT32_MAX;

		return sampler;
	}

	DescriptorView SamplerAnisotropicWrap()
	{
		const auto sampler = make_shared<Descriptor>();
		sampler->ResourceType = Descriptor::SAMPLER;
		sampler->Samp.Filter = D3D12_FILTER_ANISOTROPIC;
		sampler->Samp.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler->Samp.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler->Samp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler->Samp.MaxAnisotropy = 16;
		sampler->Samp.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		sampler->Samp.MinLOD = 0.0f;
		sampler->Samp.MaxLOD = D3D12_FLOAT32_MAX;

		return sampler;
	}

	DescriptorView SamplerAnisotropicClamp()
	{
		const auto sampler = make_shared<Descriptor>();
		sampler->ResourceType = Descriptor::SAMPLER;
		sampler->Samp.Filter = D3D12_FILTER_ANISOTROPIC;
		sampler->Samp.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		sampler->Samp.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		sampler->Samp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		sampler->Samp.MaxAnisotropy = 16;
		sampler->Samp.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		sampler->Samp.MinLOD = 0.0f;
		sampler->Samp.MaxLOD = D3D12_FLOAT32_MAX;

		return sampler;
	}

	DescriptorView SamplerAnisotropicBorder()
	{
		const auto sampler = make_shared<Descriptor>();
		sampler->ResourceType = Descriptor::SAMPLER;
		sampler->Samp.Filter = D3D12_FILTER_ANISOTROPIC;
		sampler->Samp.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler->Samp.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler->Samp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler->Samp.MaxAnisotropy = 16;
		sampler->Samp.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		sampler->Samp.MinLOD = 0.0f;
		sampler->Samp.MaxLOD = D3D12_FLOAT32_MAX;

		return sampler;
	}

	DescriptorView SamplerAnisotropicLessEqual()
	{
		const auto sampler = make_shared<Descriptor>();
		sampler->ResourceType = Descriptor::SAMPLER;
		sampler->Samp.Filter = D3D12_FILTER_ANISOTROPIC;
		sampler->Samp.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler->Samp.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler->Samp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler->Samp.BorderColor[0] = 1.0f;
		sampler->Samp.BorderColor[1] = 1.0f;
		sampler->Samp.BorderColor[2] = 1.0f;
		sampler->Samp.BorderColor[3] = 1.0f;
		sampler->Samp.MaxAnisotropy = 16;
		sampler->Samp.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		sampler->Samp.MinLOD = 0.0f;
		sampler->Samp.MaxLOD = D3D12_FLOAT32_MAX;

		return sampler;
	}
}
