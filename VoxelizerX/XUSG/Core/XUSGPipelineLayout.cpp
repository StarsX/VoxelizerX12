//--------------------------------------------------------------------------------------
// By Stars XU Tianchen
//--------------------------------------------------------------------------------------

#include "DXFrameworkHelper.h"
#include "XUSGGraphicsState.h"

using namespace std;
using namespace XUSG;

Util::PipelineLayout::PipelineLayout() :
	m_descriptorTableLayoutKeys(0),
	m_tableLayoutsCompleted(false)
{
	m_pipelineLayoutKey.resize(1);
}

Util::PipelineLayout::~PipelineLayout()
{
}

void Util::PipelineLayout::SetShaderStage(uint32_t index, Shader::Stage stage)
{
	checkKeySpace(index)[0] = stage;
}

void Util::PipelineLayout::SetRange(uint32_t index, DescriptorType type, uint32_t num, uint32_t baseReg,
	uint32_t space, uint8_t flags)
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
	pRanges[i].Flags = flags;
}

PipelineLayout Util::PipelineLayout::CreatePipelineLayout(PipelineLayoutCache &pipelineLayoutCache, uint8_t flags, const wchar_t *name)
{
	return pipelineLayoutCache.CreatePipelineLayout(*this, flags, name);
}

PipelineLayout Util::PipelineLayout::GetPipelineLayout(PipelineLayoutCache &pipelineLayoutCache, uint8_t flags, const wchar_t *name)
{
	return pipelineLayoutCache.GetPipelineLayout(*this, flags, name);
}

DescriptorTableLayout Util::PipelineLayout::CreateDescriptorTableLayout(uint32_t index, PipelineLayoutCache &pipelineLayoutCache) const
{
	return pipelineLayoutCache.CreateDescriptorTableLayout(index, *this);
}

DescriptorTableLayout Util::PipelineLayout::GetDescriptorTableLayout(uint32_t index, PipelineLayoutCache &pipelineLayoutCache) const
{
	return pipelineLayoutCache.GetDescriptorTableLayout(index, *this);
}

const vector<string> &Util::PipelineLayout::GetDescriptorTableLayoutKeys() const
{
	return m_descriptorTableLayoutKeys;
}

string &Util::PipelineLayout::GetPipelineLayoutKey(PipelineLayoutCache *pPipelineLayoutCache)
{
	if (!m_tableLayoutsCompleted && pPipelineLayoutCache)
	{
		m_pipelineLayoutKey.resize(sizeof(void*) * m_descriptorTableLayoutKeys.size() + 1);

		const auto descriptorTableLayouts = reinterpret_cast<const void**>(&m_pipelineLayoutKey[1]);

		for (auto i = 0u; i < m_descriptorTableLayoutKeys.size(); ++i)
			descriptorTableLayouts[i] = GetDescriptorTableLayout(i, *pPipelineLayoutCache).get();

		m_tableLayoutsCompleted = true;
	}

	return m_pipelineLayoutKey;
}

string &Util::PipelineLayout::checkKeySpace(uint32_t index)
{
	m_tableLayoutsCompleted = false;

	if (index >= m_descriptorTableLayoutKeys.size())
		m_descriptorTableLayoutKeys.resize(index + 1);

	if (m_descriptorTableLayoutKeys[index].empty())
		m_descriptorTableLayoutKeys[index].resize(1);

	m_descriptorTableLayoutKeys[index][0] = Shader::Stage::ALL;

	return m_descriptorTableLayoutKeys[index];
}

//--------------------------------------------------------------------------------------

PipelineLayoutCache::PipelineLayoutCache() :
	m_device(nullptr),
	m_pipelineLayouts(0),
	m_descriptorTableLayouts(0)
{
}

PipelineLayoutCache::PipelineLayoutCache(const Device &device) :
	PipelineLayoutCache()
{
	SetDevice(device);
}

PipelineLayoutCache::~PipelineLayoutCache()
{
}

void PipelineLayoutCache::SetDevice(const Device &device)
{
	m_device = device;
}

void PipelineLayoutCache::SetPipelineLayout(const string &key, const PipelineLayout &pipelineLayout)
{
	m_pipelineLayouts[key] = pipelineLayout;
}

PipelineLayout PipelineLayoutCache::CreatePipelineLayout(Util::PipelineLayout &util, uint8_t flags, const wchar_t *name)
{
	auto& pipelineLayoutKey = util.GetPipelineLayoutKey(this);
	pipelineLayoutKey[0] = flags;

	return createPipelineLayout(pipelineLayoutKey, name);
}

PipelineLayout PipelineLayoutCache::GetPipelineLayout(Util::PipelineLayout &util, uint8_t flags, const wchar_t *name)
{
	auto& pipelineLayoutKey = util.GetPipelineLayoutKey(this);
	pipelineLayoutKey[0] = flags;

	return getPipelineLayout(pipelineLayoutKey, name);
}

DescriptorTableLayout PipelineLayoutCache::CreateDescriptorTableLayout(uint32_t index, const Util::PipelineLayout &util)
{
	const auto &keys = util.GetDescriptorTableLayoutKeys();

	return keys.size() > index ? createDescriptorTableLayout(util.GetDescriptorTableLayoutKeys()[index]) : nullptr;
}

DescriptorTableLayout PipelineLayoutCache::GetDescriptorTableLayout(uint32_t index, const Util::PipelineLayout &util)
{
	const auto &keys = util.GetDescriptorTableLayoutKeys();

	return keys.size() > index ? getDescriptorTableLayout(util.GetDescriptorTableLayoutKeys()[index]) : nullptr;
}

PipelineLayout PipelineLayoutCache::createPipelineLayout(const string &key, const wchar_t *name) const
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
	H_RETURN(D3DX12SerializeVersionedRootSignature(&layoutDesc, featureData.HighestVersion, &signature, &error),
		cerr, reinterpret_cast<char*>(error->GetBufferPointer()), nullptr);

	PipelineLayout layout;
	H_RETURN(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
		IID_PPV_ARGS(&layout)), cerr, reinterpret_cast<char*>(error->GetBufferPointer()), nullptr);
	if (name) layout->SetName(name);

	return layout;
}

PipelineLayout PipelineLayoutCache::getPipelineLayout(const string &key, const wchar_t *name)
{
	const auto layoutIter = m_pipelineLayouts.find(key);

	// Create one, if it does not exist
	if (layoutIter == m_pipelineLayouts.end())
	{
		const auto layout = createPipelineLayout(key, name);
		m_pipelineLayouts[key] = layout;

		return layout;
	}

	return layoutIter->second;
}

DescriptorTableLayout PipelineLayoutCache::createDescriptorTableLayout(const string &key)
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
			range.BaseRegister, range.Space, D3D12_DESCRIPTOR_RANGE_FLAGS(range.Flags));
	}

	D3D12_SHADER_VISIBILITY visibilities[Shader::NUM_STAGE];
	visibilities[Shader::Stage::VS] = D3D12_SHADER_VISIBILITY_VERTEX;
	visibilities[Shader::Stage::PS] = D3D12_SHADER_VISIBILITY_PIXEL;
	visibilities[Shader::Stage::DS] = D3D12_SHADER_VISIBILITY_DOMAIN;
	visibilities[Shader::Stage::HS] = D3D12_SHADER_VISIBILITY_HULL;
	visibilities[Shader::Stage::GS] = D3D12_SHADER_VISIBILITY_GEOMETRY;
	visibilities[Shader::Stage::ALL] = D3D12_SHADER_VISIBILITY_ALL;

	// Set param
	const auto stage = static_cast<Shader::Stage>(key[0]);
	layout->InitAsDescriptorTable(numRanges, ranges.data(), visibilities[stage]);

	return layout;
}

DescriptorTableLayout PipelineLayoutCache::getDescriptorTableLayout(const string &key)
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
