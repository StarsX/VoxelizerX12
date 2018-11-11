//--------------------------------------------------------------------------------------
// By Stars XU Tianchen
//--------------------------------------------------------------------------------------

#include "DXFrameworkHelper.h"
#include "XUSGGraphicsState.h"

using namespace std;
using namespace XUSG;

Util::PipelineLayout::PipelineLayout() :
	m_descriptorTableLayoutKeys(0)
{
	m_pipelineLayoutKey.resize(1);
}

Util::PipelineLayout::~PipelineLayout()
{
}

void Util::PipelineLayout::SetShaderStage(uint32_t index, Shader::Stage::Type stage)
{
	checkKeySpace(index)[0] = stage;
}

void Util::PipelineLayout::SetRange(uint32_t index, DescriptorType type, uint32_t num, uint32_t baseReg, uint32_t space)
{
	auto &key = checkKeySpace(index);

	// Append
	const auto i = (key.size() - 1) / sizeof(DescriptorRange);
	key.resize(key.size() + sizeof(DescriptorRange));

	// Interpret key data as ranges
	const auto pRanges = reinterpret_cast<DescriptorRange*>(&key[1]);

	// Fill key entries
	pRanges[i].ViewType = type;
	pRanges[i].NumDescriptors = num;
	pRanges[i].BaseRegister = baseReg;
	pRanges[i].Space = space;
}

PipelineLayout Util::PipelineLayout::CreatePipelineLayout(PipelineLayoutPool &pipelineLayoutPool, uint8_t flags)
{
	setPipelineLayoutKey(pipelineLayoutPool, flags);

	return pipelineLayoutPool.createPipelineLayout(m_pipelineLayoutKey);
}

PipelineLayout Util::PipelineLayout::CreatePipelineLayout(Graphics::Pipeline::Pool &pipelinePool, uint8_t flags)
{
	return CreatePipelineLayout(pipelinePool.GetPipelineLayoutPool(), flags);
}

PipelineLayout Util::PipelineLayout::GetPipelineLayout(PipelineLayoutPool &pipelineLayoutPool, uint8_t flags)
{
	setPipelineLayoutKey(pipelineLayoutPool, flags);
	
	return pipelineLayoutPool.getPipelineLayout(m_pipelineLayoutKey);
}

PipelineLayout Util::PipelineLayout::GetPipelineLayout(Graphics::Pipeline::Pool &pipelinePool, uint8_t flags)
{
	return GetPipelineLayout(pipelinePool.GetPipelineLayoutPool(), flags);
}

DescriptorTableLayout Util::PipelineLayout::CreateDescriptorTableLayout(uint32_t index, PipelineLayoutPool &pipelineLayoutPool) const
{
	return pipelineLayoutPool.createDescriptorTableLayout(m_descriptorTableLayoutKeys[index]);
}

DescriptorTableLayout Util::PipelineLayout::CreateDescriptorTableLayout(uint32_t index, Graphics::Pipeline::Pool &pipelinePool) const
{
	return CreateDescriptorTableLayout(index, pipelinePool.GetPipelineLayoutPool());
}

DescriptorTableLayout Util::PipelineLayout::GetDescriptorTableLayout(uint32_t index, PipelineLayoutPool &pipelineLayoutPool) const
{
	return pipelineLayoutPool.getDescriptorTableLayout(m_descriptorTableLayoutKeys[index]);
}

DescriptorTableLayout Util::PipelineLayout::GetDescriptorTableLayout(uint32_t index, Graphics::Pipeline::Pool &pipelinePool) const
{
	return GetDescriptorTableLayout(index, pipelinePool.GetPipelineLayoutPool());
}

void Util::PipelineLayout::setPipelineLayoutKey(PipelineLayoutPool &pipelineLayoutPool, uint8_t flags)
{
	m_pipelineLayoutKey.resize(sizeof(void*) * m_descriptorTableLayoutKeys.size() + 1);
	m_pipelineLayoutKey[0] = flags;

	const auto descriptorTableLayouts = reinterpret_cast<const void**>(&m_pipelineLayoutKey[1]);

	for (auto i = 0u; i < m_descriptorTableLayoutKeys.size(); ++i)
		descriptorTableLayouts[i] = pipelineLayoutPool.getDescriptorTableLayout(m_descriptorTableLayoutKeys[i]).get();
}

string &Util::PipelineLayout::checkKeySpace(uint32_t index)
{
	if (index >= m_descriptorTableLayoutKeys.size())
		m_descriptorTableLayoutKeys.resize(index + 1);

	if (m_descriptorTableLayoutKeys[index].empty())
		m_descriptorTableLayoutKeys[index].resize(1);

	return m_descriptorTableLayoutKeys[index];
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

PipelineLayoutPool::PipelineLayoutPool() :
	m_device(nullptr),
	m_pipelineLayouts(0),
	m_descriptorTableLayouts(0)
{
}

PipelineLayoutPool::PipelineLayoutPool(const Device &device) :
	PipelineLayoutPool()
{
	SetDevice(device);
}

PipelineLayoutPool::~PipelineLayoutPool()
{
}

void PipelineLayoutPool::SetDevice(const Device &device)
{
	m_device = device;
}

PipelineLayout PipelineLayoutPool::GetPipelineLayout(Util::PipelineLayout &util, uint8_t flags)
{
	util.setPipelineLayoutKey(*this, flags);

	return getPipelineLayout(util.m_pipelineLayoutKey);
}

DescriptorTableLayout PipelineLayoutPool::CreateDescriptorTableLayout(uint32_t index, const Util::PipelineLayout &util)
{
	return createDescriptorTableLayout(util.m_descriptorTableLayoutKeys[index]);
}

DescriptorTableLayout PipelineLayoutPool::GetDescriptorTableLayout(uint32_t index, const Util::PipelineLayout &util)
{
	return getDescriptorTableLayout(util.m_descriptorTableLayoutKeys[index]);
}

PipelineLayout PipelineLayoutPool::createPipelineLayout(const string &key) const
{
	D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

	// This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
	featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

	if (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;

	const auto numLayouts = static_cast<uint32_t>((key.size() - 1) / sizeof(void*));
	const auto flags = static_cast<D3D12_ROOT_SIGNATURE_FLAGS>(key[0]);
	const auto pDescriptorTableLayoutPtrs = reinterpret_cast<DescriptorTableLayout::element_type* const*>(&key[1]);

	vector<D3D12_ROOT_PARAMETER1> descriptorTableLayouts(numLayouts);
	for (auto i = 0u; i < numLayouts; ++i)
		descriptorTableLayouts[i] = *pDescriptorTableLayoutPtrs[i];

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC layoutDesc;
	layoutDesc.Init_1_1(numLayouts, descriptorTableLayouts.data(), 0, nullptr, static_cast<D3D12_ROOT_SIGNATURE_FLAGS>(flags));

	Blob signature, error;
	ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&layoutDesc, featureData.HighestVersion, &signature, &error));

	PipelineLayout layout;
	ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&layout)));

	return layout;
}

PipelineLayout PipelineLayoutPool::getPipelineLayout(const string &key)
{
	const auto layoutIter = m_pipelineLayouts.find(key);

	// Create one, if it does not exist
	if (layoutIter == m_pipelineLayouts.end())
	{
		const auto layout = createPipelineLayout(key);
		m_pipelineLayouts[key] = layout;

		return layout;
	}

	return layoutIter->second;
}

DescriptorTableLayout PipelineLayoutPool::createDescriptorTableLayout(const string &key)
{
	D3D12_DESCRIPTOR_RANGE_TYPE rangeTypes[static_cast<uint8_t>(DescriptorType::NUM)];
	rangeTypes[static_cast<uint8_t>(DescriptorType::SRV)] = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	rangeTypes[static_cast<uint8_t>(DescriptorType::UAV)] = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	rangeTypes[static_cast<uint8_t>(DescriptorType::CBV)] = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	rangeTypes[static_cast<uint8_t>(DescriptorType::SAMPLER)] = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;

	// Create descriptor table layout
	auto layout = make_shared<DescriptorTableLayout::element_type>();

	// Set ranges
	const auto numRanges = static_cast<uint32_t>((key.size() - 1) / sizeof(DescriptorRange));
	const auto pRanges = reinterpret_cast<const DescriptorRange*>(&key[1]);

	layout->ranges = DescriptorRangeList(numRanges);
	auto &ranges = layout->ranges;

	for (auto i = 0u; i < numRanges; ++i)
	{
		const auto &range = pRanges[i];
		ranges[i].Init(rangeTypes[static_cast<uint8_t>(range.ViewType)], range.NumDescriptors,
			range.BaseRegister, range.Space, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);
		//	range.Space, range.DescriptorType == Descriptor::SAMPLER ?
		//	D3D12_DESCRIPTOR_RANGE_FLAG_NONE : D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
	}

	D3D12_SHADER_VISIBILITY visibilities[Shader::Stage::NUM];
	visibilities[Shader::Stage::VS] = D3D12_SHADER_VISIBILITY_VERTEX;
	visibilities[Shader::Stage::PS] = D3D12_SHADER_VISIBILITY_PIXEL;
	visibilities[Shader::Stage::DS] = D3D12_SHADER_VISIBILITY_DOMAIN;
	visibilities[Shader::Stage::HS] = D3D12_SHADER_VISIBILITY_HULL;
	visibilities[Shader::Stage::GS] = D3D12_SHADER_VISIBILITY_GEOMETRY;
	visibilities[Shader::Stage::ALL] = D3D12_SHADER_VISIBILITY_ALL;

	// Set param
	const auto stage = static_cast<Shader::Stage::Type>(key[0]);
	layout->InitAsDescriptorTable(numRanges, ranges.data(), visibilities[stage]);

	return layout;
}

DescriptorTableLayout PipelineLayoutPool::getDescriptorTableLayout(const string &key)
{
	const auto layoutPtrIter = m_descriptorTableLayouts.find(key);

	// Create one, if it does not exist
	if (layoutPtrIter == m_descriptorTableLayouts.end())
	{
		const auto layout = createDescriptorTableLayout(key);
		m_descriptorTableLayouts[key] = layout;

		return layout;
	}

	return layoutPtrIter->second;
}
