//--------------------------------------------------------------------------------------
// By Stars XU Tianchen
//--------------------------------------------------------------------------------------

#include "DXFrameworkHelper.h"
#include "XUSGDescriptor.h"
#include "XUSGSampler.inl"

using namespace std;
using namespace XUSG;

Util::DescriptorTable::DescriptorTable()
{
	m_key.resize(0);
}

Util::DescriptorTable::~DescriptorTable()
{
}

void Util::DescriptorTable::SetDescriptors(uint32_t start, uint32_t num, const Descriptor *srcDescriptors)
{
	const auto size = sizeof(void*) * (start + num);
	if (size > m_key.size())
		m_key.resize(size);

	const auto descriptors = reinterpret_cast<Descriptor*>(&m_key[0]);
	memcpy(&descriptors[start], srcDescriptors, sizeof(Descriptor*) * num);
}

void Util::DescriptorTable::SetSamplers(uint32_t start, uint32_t num,
	const SamplerPreset::Type *presets, DescriptorTablePool &descriptorTablePool)
{
	const auto size = sizeof(void*) * (start + num);
	if (size > m_key.size())
		m_key.resize(size);

	const auto descriptors = reinterpret_cast<const Sampler**>(&m_key[0]);

	for (auto i = 0u; i < num; ++i)
		descriptors[start + i] = descriptorTablePool.GetSampler(presets[i]).get();
}

DescriptorTable Util::DescriptorTable::CreateCbvSrvUavTable(DescriptorTablePool &descriptorTablePool)
{
	return descriptorTablePool.createCbvSrvUavTable(m_key);
}

DescriptorTable Util::DescriptorTable::GetCbvSrvUavTable(DescriptorTablePool &descriptorTablePool)
{
	return descriptorTablePool.getCbvSrvUavTable(m_key);
}

DescriptorTable Util::DescriptorTable::CreateSamplerTable(DescriptorTablePool &descriptorTablePool)
{
	return descriptorTablePool.createSamplerTable(m_key);
}

DescriptorTable Util::DescriptorTable::GetSamplerTable(DescriptorTablePool &descriptorTablePool)
{
	return descriptorTablePool.getSamplerTable(m_key);
}

RenderTargetTable Util::DescriptorTable::CreateRtvTable(DescriptorTablePool &descriptorTablePool)
{
	return descriptorTablePool.createRtvTable(m_key);
}

RenderTargetTable Util::DescriptorTable::GetRtvTable(DescriptorTablePool &descriptorTablePool)
{
	return descriptorTablePool.getRtvTable(m_key);
}

//--------------------------------------------------------------------------------------

DescriptorTablePool::DescriptorTablePool() :
	m_device(nullptr),
	m_cbvSrvUavTables(0),
	m_samplerTables(0),
	m_rtvTables(0),
	m_cbvSrvUavPool(nullptr),
	m_samplerPool(nullptr),
	m_rtvPool(nullptr),
	m_numCbvSrvUavs(0),
	m_numSamplers(0),
	m_numRtvs(0),
	m_samplerPresets()
{
	// Sampler presets
	m_pfnSamplers[SamplerPreset::POINT_WRAP] = SamplerPointWrap;
	m_pfnSamplers[SamplerPreset::POINT_CLAMP] = SamplerPointClamp;
	m_pfnSamplers[SamplerPreset::POINT_BORDER] = SamplerPointBorder;
	m_pfnSamplers[SamplerPreset::POINT_LESS_EQUAL] = SamplerPointLessEqual;

	m_pfnSamplers[SamplerPreset::LINEAR_WRAP] = SamplerLinearWrap;
	m_pfnSamplers[SamplerPreset::LINEAR_CLAMP] = SamplerLinearClamp;
	m_pfnSamplers[SamplerPreset::LINEAR_BORDER] = SamplerLinearBorder;
	m_pfnSamplers[SamplerPreset::LINEAR_LESS_EQUAL] = SamplerLinearLessEqual;

	m_pfnSamplers[SamplerPreset::ANISOTROPIC_WRAP] = SamplerAnisotropicWrap;
	m_pfnSamplers[SamplerPreset::ANISOTROPIC_CLAMP] = SamplerAnisotropicClamp;
	m_pfnSamplers[SamplerPreset::ANISOTROPIC_BORDER] = SamplerAnisotropicBorder;
	m_pfnSamplers[SamplerPreset::ANISOTROPIC_LESS_EQUAL] = SamplerAnisotropicLessEqual;
}

DescriptorTablePool::DescriptorTablePool(const Device &device) :
	DescriptorTablePool()
{
	SetDevice(device);
}

DescriptorTablePool::~DescriptorTablePool()
{
}

void DescriptorTablePool::SetDevice(const Device &device)
{
	m_device = device;

	m_strideCbvSrvUav = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	m_strideSampler = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
	m_strideRtv = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
}

void DescriptorTablePool::AllocateCbvSrvUavPool(uint32_t numDescriptors)
{
	allocateCbvSrvUavPool(numDescriptors);
}

void DescriptorTablePool::AllocateSamplerPool(uint32_t numDescriptors)
{
	allocateSamplerPool(numDescriptors);
}

void DescriptorTablePool::AllocateRtvPool(uint32_t numDescriptors)
{
	allocateRtvPool(numDescriptors);
}

DescriptorTable DescriptorTablePool::CreateCbvSrvUavTable(const Util::DescriptorTable &util)
{
	return createCbvSrvUavTable(util.m_key);
}

DescriptorTable DescriptorTablePool::GetCbvSrvUavTable(const Util::DescriptorTable &util)
{
	return getCbvSrvUavTable(util.m_key);
}

DescriptorTable DescriptorTablePool::CreateSamplerTable(const Util::DescriptorTable &util)
{
	return createSamplerTable(util.m_key);
}

DescriptorTable DescriptorTablePool::GetSamplerTable(const Util::DescriptorTable &util)
{
	return getSamplerTable(util.m_key);
}

RenderTargetTable DescriptorTablePool::CreateRtvTable(const Util::DescriptorTable &util)
{
	return createRtvTable(util.m_key);
}

RenderTargetTable DescriptorTablePool::GetRtvTable(const Util::DescriptorTable &util)
{
	return getRtvTable(util.m_key);
}

const DescriptorPool &DescriptorTablePool::GetCbvSrvUavPool() const
{
	return m_cbvSrvUavPool;
}

const DescriptorPool &DescriptorTablePool::GetSamplerPool() const
{
	return m_samplerPool;
}

const shared_ptr<Sampler> &DescriptorTablePool::GetSampler(SamplerPreset::Type preset)
{
	if (m_samplerPresets[preset] == nullptr)
		m_samplerPresets[preset] = make_shared<Sampler>(m_pfnSamplers[preset]());

	return m_samplerPresets[preset];
}

bool DescriptorTablePool::allocateCbvSrvUavPool(uint32_t numDescriptors)
{
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = numDescriptors;
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	V_RETURN(m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_cbvSrvUavPool)), cerr, false);

	m_numCbvSrvUavs = 0;

	return true;
}

bool DescriptorTablePool::allocateSamplerPool(uint32_t numDescriptors)
{
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = numDescriptors;
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	V_RETURN(m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_samplerPool)), cerr, false);

	m_numSamplers = 0;

	return true;
}

bool DescriptorTablePool::allocateRtvPool(uint32_t numDescriptors)
{
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = numDescriptors;
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	V_RETURN(m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_rtvPool)), cerr, false);
	
	m_numRtvs = 0;

	return true;
}

bool DescriptorTablePool::reallocateCbvSrvUavPool(const string &key)
{
	assert(key.size() > 0);
	const auto numDescriptors = static_cast<uint32_t>(key.size() / sizeof(size_t));

#if 0
	// Record source data
	const auto srcPool = m_cbvSrvUavPool;
	const auto numCbvSrcUavs = m_numCbvSrvUavs;

	// Allocate a new pool
	m_numCbvSrvUavs += numDescriptors;
	allocateCbvSrvUavPool(m_numCbvSrvUavs);

	// Copy descriptors
	m_device->CopyDescriptorsSimple(numCbvSrcUavs, m_cbvSrvUavPool->GetCPUDescriptorHandleForHeapStart(),
		srcPool->GetCPUDescriptorHandleForHeapStart(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	m_numCbvSrvUavs = numCbvSrcUavs;
#endif

	// Allocate a new pool
	m_numCbvSrvUavs += numDescriptors;
	N_RETURN(allocateCbvSrvUavPool(m_numCbvSrvUavs), false);

	// Recreate descriptor tables
	for (auto &tableIter : m_cbvSrvUavTables)
	{
		const auto table = createCbvSrvUavTable(tableIter.first);
		*tableIter.second = *table;
	}

	return true;
}

bool DescriptorTablePool::reallocateSamplerPool(const string &key)
{
	assert(key.size() > 0);
	const auto numDescriptors = static_cast<uint32_t>(key.size() / sizeof(size_t));

	// Allocate a new pool
	m_numSamplers += numDescriptors;
	N_RETURN(allocateSamplerPool(m_numSamplers), false);

	// Recreate descriptor tables
	for (auto &tableIter : m_samplerTables)
	{
		const auto table = createSamplerTable(tableIter.first);
		*tableIter.second = *table;
	}

	return true;
}

bool DescriptorTablePool::reallocateRtvPool(const string &key)
{
	assert(key.size() > 0);
	const auto numDescriptors = static_cast<uint32_t>(key.size() / sizeof(size_t));

	// Allocate a new pool
	m_numRtvs += numDescriptors;
	N_RETURN(allocateRtvPool(m_numRtvs), false);

	// Recreate descriptor tables
	for (auto &tableIter : m_rtvTables)
	{
		const auto table = createRtvTable(tableIter.first);
		*tableIter.second = *table;
	}

	return true;
}

DescriptorTable DescriptorTablePool::createCbvSrvUavTable(const string &key)
{
	if (key.size() > 0)
	{
		const auto numDescriptors = static_cast<uint32_t>(key.size() / sizeof(size_t));
		const auto descriptors = reinterpret_cast<const Descriptor*>(&key[0]);

		// Compute start addresses for CPU and GPU handles
		Descriptor descriptor(m_cbvSrvUavPool->GetCPUDescriptorHandleForHeapStart(), m_numCbvSrvUavs, m_strideCbvSrvUav);
		DescriptorTable table = make_shared<DescriptorView>(m_cbvSrvUavPool->GetGPUDescriptorHandleForHeapStart(),
			m_numCbvSrvUavs, m_strideCbvSrvUav);

		// Create a descriptor table
		for (auto i = 0u; i < numDescriptors; ++i)
		{
			// Copy a descriptor
			m_device->CopyDescriptorsSimple(1, descriptor, descriptors[i], D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			descriptor.Offset(m_strideCbvSrvUav);
			++m_numCbvSrvUavs;
		}

		return table;
	}

	return nullptr;
}

DescriptorTable DescriptorTablePool::getCbvSrvUavTable(const string &key)
{
	if (key.size() > 0)
	{
		const auto tableIter = m_cbvSrvUavTables.find(key);

		// Create one, if it does not exist
		if (tableIter == m_cbvSrvUavTables.end() && reallocateCbvSrvUavPool(key))
		{
			const auto table = createCbvSrvUavTable(key);
			m_cbvSrvUavTables[key] = table;

			return table;
		}

		return tableIter->second;
	}

	return nullptr;
}

DescriptorTable DescriptorTablePool::createSamplerTable(const string &key)
{
	if (key.size() > 0)
	{
		const auto numDescriptors = static_cast<uint32_t>(key.size() / sizeof(void*));
		const auto descriptors = reinterpret_cast<const Sampler* const*>(&key[0]);

		// Compute start addresses for CPU and GPU handles
		Descriptor descriptor(m_samplerPool->GetCPUDescriptorHandleForHeapStart(), m_numSamplers, m_strideSampler);
		DescriptorTable table = make_shared<DescriptorView>(m_samplerPool->GetGPUDescriptorHandleForHeapStart(),
			m_numSamplers, m_strideSampler);
		
		// Create a descriptor table
		for (auto i = 0u; i < numDescriptors; ++i)
		{
			// Copy a descriptor
			m_device->CreateSampler(descriptors[i], descriptor);
			descriptor.Offset(m_strideSampler);
			++m_numSamplers;
		}

		return table;
	}

	return nullptr;
}

DescriptorTable DescriptorTablePool::getSamplerTable(const string &key)
{
	if (key.size() > 0)
	{
		const auto tableIter = m_samplerTables.find(key);

		// Create one, if it does not exist
		if (tableIter == m_samplerTables.end() && reallocateSamplerPool(key))
		{
			const auto table = createSamplerTable(key);
			m_samplerTables[key] = table;

			return table;
		}

		return tableIter->second;
	}

	return nullptr;
}

RenderTargetTable DescriptorTablePool::createRtvTable(const string &key)
{
	if (key.size() > 0)
	{
		const auto numDescriptors = static_cast<uint32_t>(key.size() / sizeof(void*));
		const auto descriptors = reinterpret_cast<const Descriptor*>(&key[0]);

		// Compute start addresses for CPU and GPU handles
		Descriptor descriptor(m_rtvPool->GetCPUDescriptorHandleForHeapStart(), m_numRtvs, m_strideRtv);
		RenderTargetTable table = make_shared<Descriptor>();
		*table = descriptor;

		// Create a descriptor table
		for (auto i = 0u; i < numDescriptors; ++i)
		{
			// Copy a descriptor
			m_device->CopyDescriptorsSimple(1, descriptor, descriptors[i], D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
			descriptor.Offset(m_strideRtv);
			++m_numRtvs;
		}

		return table;
	}

	return nullptr;
}

RenderTargetTable DescriptorTablePool::getRtvTable(const string &key)
{
	if (key.size() > 0)
	{
		const auto tableIter = m_rtvTables.find(key);

		// Create one, if it does not exist
		if (tableIter == m_rtvTables.end() && reallocateRtvPool(key))
		{
			const auto table = createRtvTable(key);
			m_rtvTables[key] = table;

			return table;
		}

		return tableIter->second;
	}

	return nullptr;
}
