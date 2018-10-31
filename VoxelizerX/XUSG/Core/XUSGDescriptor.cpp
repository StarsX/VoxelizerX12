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

void Util::DescriptorTable::SetDescriptors(uint32_t start, uint32_t num, const DescriptorView *descriptorViews)
{
	const auto size = sizeof(void*) * (start + num);
	if (size > m_key.size())
		m_key.resize(size);

	const auto descriptors = reinterpret_cast<const void**>(&m_key[0]);

	for (auto i = 0u; i < num; ++i)
		descriptors[start + i] = descriptorViews[i].get();
}

void Util::DescriptorTable::SetSamplers(uint32_t start, uint32_t num,
	const SamplerPreset::Type *presets, DescriptorTablePool& descriptorTablePool)
{
	vector<DescriptorView> samplers(num);

	for (auto i = 0u; i < num; ++i)
		samplers[i] = descriptorTablePool.GetSampler(presets[i]);

	SetDescriptors(start, num, samplers.data());
}

void Util::DescriptorTable::SetDepthStencil(const DescriptorView& dsv)
{
	m_key.resize(sizeof(void*));

	auto& descriptor = reinterpret_cast<const void*&>(m_key[0]);

	descriptor = dsv.get();
}

DescriptorTable Util::DescriptorTable::CreateDescriptorTable(DescriptorTablePool& descriptorTablePool)
{
	return descriptorTablePool.createDescriptorTable(m_key);
}

DescriptorTable Util::DescriptorTable::GetDescriptorTable(DescriptorTablePool& descriptorTablePool)
{
	return descriptorTablePool.getDescriptorTable(m_key);
}

RenderTargetTable Util::DescriptorTable::CreateRenderTargetTable(DescriptorTablePool& descriptorTablePool)
{
	return descriptorTablePool.createRenderTargetTable(m_key);
}

RenderTargetTable Util::DescriptorTable::GetRenderTargetTable(DescriptorTablePool& descriptorTablePool)
{
	return descriptorTablePool.getRenderTargetTable(m_key);
}

DepthStencilHandle Util::DescriptorTable::CreateDepthStencilHandle(DescriptorTablePool& descriptorTablePool)
{
	return descriptorTablePool.createDepthStencilHandle(m_key);
}

DepthStencilHandle Util::DescriptorTable::GetDepthStencilHandle(DescriptorTablePool& descriptorTablePool)
{
	return descriptorTablePool.getDepthStencilHandle(m_key);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DescriptorTablePool::DescriptorTablePool() :
	m_srvUavCbvTables(0),
	m_samplerTables(0),
	m_numSrvUavCbvs(0),
	m_numSamplers(0)
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

DescriptorTablePool::DescriptorTablePool(const Device& device) :
	DescriptorTablePool()
{
	SetDevice(device);
}

DescriptorTablePool::~DescriptorTablePool()
{
}

void DescriptorTablePool::SetDevice(const Device& device)
{
	m_device = device;

	m_strideSrvUavCbv = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	m_strideSampler = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
	m_strideRtv = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	m_strideDsv = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
}

void DescriptorTablePool::AllocateSrvUavCbvPool(uint32_t numDescriptors)
{
	allocateSrvUavCbvPool(numDescriptors);
}

void DescriptorTablePool::AllocateSamplerPool(uint32_t numDescriptors)
{
	allocateSamplerPool(numDescriptors);
}

void DescriptorTablePool::AllocateRtvPool(uint32_t numDescriptors)
{
	allocateRtvPool(numDescriptors);
}

void DescriptorTablePool::AllocateDsvPool(uint32_t numDescriptors)
{
	allocateDsvPool(numDescriptors);
}

DescriptorTable DescriptorTablePool::CreateDescriptorTable(const Util::DescriptorTable& util)
{
	return createDescriptorTable(util.m_key);
}

DescriptorTable DescriptorTablePool::GetDescriptorTable(const Util::DescriptorTable& util)
{
	return getDescriptorTable(util.m_key);
}

RenderTargetTable DescriptorTablePool::CreateRenderTargetTable(const Util::DescriptorTable& util)
{
	return createRenderTargetTable(util.m_key);
}

RenderTargetTable DescriptorTablePool::GetRenderTargetTable(const Util::DescriptorTable& util)
{
	return getRenderTargetTable(util.m_key);
}

DepthStencilHandle DescriptorTablePool::CreateDepthStencilHandle(const Util::DescriptorTable& util)
{
	return createDepthStencilHandle(util.m_key);
}

DepthStencilHandle DescriptorTablePool::GetDepthStencilHandle(const Util::DescriptorTable& util)
{
	return getDepthStencilHandle(util.m_key);
}

const DescriptorPool& DescriptorTablePool::GetSrvUavCbvPool() const
{
	return m_srvUavCbvPool;
}

const DescriptorPool& DescriptorTablePool::GetSamplerPool() const
{
	return m_samplerPool;
}

const DescriptorView& DescriptorTablePool::GetSampler(SamplerPreset::Type preset)
{
	if (m_samplerPresets[preset] == nullptr)
		m_samplerPresets[preset] = m_pfnSamplers[preset]();

	return m_samplerPresets[preset];
}

void DescriptorTablePool::allocateSrvUavCbvPool(uint32_t numDescriptors)
{
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = numDescriptors;
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_srvUavCbvPool)));

	m_numSrvUavCbvs = 0;
}

void DescriptorTablePool::allocateSamplerPool(uint32_t numDescriptors)
{
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = numDescriptors;
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_samplerPool)));

	m_numSamplers = 0;
}

void DescriptorTablePool::allocateRtvPool(uint32_t numDescriptors)
{
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = numDescriptors;
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ThrowIfFailed(m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_rtvPool)));

	m_numRtvs = 0;
}

void DescriptorTablePool::allocateDsvPool(uint32_t numDescriptors)
{
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = numDescriptors;
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ThrowIfFailed(m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_dsvPool)));

	m_numDsvs = 0;
}

void DescriptorTablePool::reallocateDescriptorPool(const string& key)
{
	assert(key.size() > 0);
	const auto numDescriptors = static_cast<uint32_t>(key.size() / sizeof(void*));
	const auto descriptor = *reinterpret_cast<Descriptor* const*>(&key[0]);

	// Reallocate pool
	switch (descriptor->ResourceType)
	{
	case Descriptor::SAMPLER:
		m_numSamplers += numDescriptors;
		allocateSamplerPool(m_numSamplers);

		// Recreate descriptor tables
		for (auto& tableIter : m_samplerTables)
		{
			const auto table = createDescriptorTable(tableIter.first);
			*tableIter.second = *table;
		}
		break;
	case Descriptor::RTV:
		m_numRtvs += numDescriptors;
		allocateRtvPool(m_numRtvs);

		// Recreate descriptor tables
		for (auto& tableIter : m_rtvTables)
		{
			const auto table = createRenderTargetTable(tableIter.first);
			*tableIter.second = *table;
		}
		break;
	case Descriptor::DSV:
		m_numDsvs += numDescriptors;
		allocateDsvPool(m_numDsvs);

		// Recreate descriptor tables
		for (auto& tableIter : m_dsvHandles)
		{
			const auto table = createRenderTargetTable(tableIter.first);
			*tableIter.second = *table;
		}
		break;
	default:
		m_numSrvUavCbvs += numDescriptors;
		allocateSrvUavCbvPool(m_numSrvUavCbvs);

		// Recreate descriptor tables
		for (auto& tableIter : m_srvUavCbvTables)
		{
			const auto table = createDescriptorTable(tableIter.first);
			*tableIter.second = *table;
		}
	}
}

DescriptorTable DescriptorTablePool::createDescriptorTable(const string& key)
{
	if (key.size() > 0)
	{
		const auto numDescriptors = static_cast<uint32_t>(key.size() / sizeof(void*));
		const auto descriptors = reinterpret_cast<const Descriptor* const*>(&key[0]);

		DescriptorHandleCPU handleCPU;
		DescriptorTable table = make_shared<DescriptorHandleGPU>();
		
		// Compute start addresses for CPU and GPU handles
		if (descriptors[0]->ResourceType == Descriptor::SAMPLER)
		{
			handleCPU.InitOffsetted(m_samplerPool->GetCPUDescriptorHandleForHeapStart(), m_numSamplers, m_strideSampler);
			table->InitOffsetted(m_samplerPool->GetGPUDescriptorHandleForHeapStart(), m_numSamplers, m_strideSampler);
		}
		else
		{
			handleCPU.InitOffsetted(m_srvUavCbvPool->GetCPUDescriptorHandleForHeapStart(), m_numSrvUavCbvs, m_strideSrvUavCbv);
			table->InitOffsetted(m_srvUavCbvPool->GetGPUDescriptorHandleForHeapStart(), m_numSrvUavCbvs, m_strideSrvUavCbv);
		}

		// Create a descriptor
		for (auto i = 0u; i < numDescriptors; ++i)
		{
			const auto& descriptor = descriptors[i];

			switch (descriptor->ResourceType)
			{
			case Descriptor::SRV:
				m_device->CreateShaderResourceView(descriptor->pResource.Get(), &descriptor->Srv, handleCPU);
				handleCPU.Offset(m_strideSrvUavCbv);
				++m_numSrvUavCbvs;
				break;
			case Descriptor::UAV:
				m_device->CreateUnorderedAccessView(descriptor->pResource.Get(),
					descriptor->pCounter.Get(), &descriptor->Uav, handleCPU);
				handleCPU.Offset(m_strideSrvUavCbv);
				++m_numSrvUavCbvs;
				break;
			case Descriptor::CBV:
				m_device->CreateConstantBufferView(&descriptor->Cbv, handleCPU);
				handleCPU.Offset(m_strideSrvUavCbv);
				++m_numSrvUavCbvs;
				break;
			case Descriptor::SAMPLER:
				m_device->CreateSampler(&descriptor->Samp, handleCPU);
				handleCPU.Offset(m_strideSampler);
				++m_numSamplers;
				break;
			}
		}

		return table;
	}

	return nullptr;
}

DescriptorTable DescriptorTablePool::getDescriptorTable(const string& key)
{
	if (key.size() > 0)
	{
		const auto descriptors = reinterpret_cast<const Descriptor* const*>(&key[0]);
		auto& tables = descriptors[0]->ResourceType == Descriptor::SAMPLER ?
			m_samplerTables : m_srvUavCbvTables;

		const auto tableIter = tables.find(key);

		// Create one, if it does not exist
		if (tableIter == tables.end())
		{
			reallocateDescriptorPool(key);

			const auto table = createDescriptorTable(key);
			tables[key] = table;

			return table;
		}

		return tableIter->second;
	}

	return nullptr;
}

RenderTargetTable DescriptorTablePool::createRenderTargetTable(const string& key)
{
	if (key.size() > 0)
	{
		const auto numDescriptors = static_cast<uint32_t>(key.size() / sizeof(void*));
		const auto descriptors = reinterpret_cast<const Descriptor* const*>(&key[0]);

		DescriptorHandleCPU handleCPU;
		RenderTargetTable table = make_shared<DescriptorHandleCPU>();

		// Compute start addresses for CPU and GPU handles
		handleCPU.InitOffsetted(m_rtvPool->GetCPUDescriptorHandleForHeapStart(), m_numRtvs, m_strideRtv);
		*table = handleCPU;

		// Create a descriptor
		for (auto i = 0u; i < numDescriptors; ++i)
		{
			const auto& descriptor = descriptors[i];

			m_device->CreateRenderTargetView(descriptor->pResource.Get(), descriptor->Rtv.Format ?
				&descriptor->Rtv : nullptr, handleCPU);
			handleCPU.Offset(m_strideRtv);
			++m_numRtvs;
		}

		return table;
	}

	return nullptr;
}

RenderTargetTable DescriptorTablePool::getRenderTargetTable(const string& key)
{
	if (key.size() > 0)
	{
		const auto tableIter = m_rtvTables.find(key);

		// Create one, if it does not exist
		if (tableIter == m_rtvTables.end())
		{
			reallocateDescriptorPool(key);

			const auto table = createRenderTargetTable(key);
			m_rtvTables[key] = table;

			return table;
		}

		return tableIter->second;
	}

	return nullptr;
}

DepthStencilHandle DescriptorTablePool::createDepthStencilHandle(const string& key)
{
	if (key.size() > 0)
	{
		const auto numDescriptors = static_cast<uint32_t>(key.size() / sizeof(void*));
		const auto descriptors = reinterpret_cast<const Descriptor* const*>(&key[0]);

		DescriptorHandleCPU handleCPU;
		DepthStencilHandle handle = make_shared<DescriptorHandleCPU>();

		// Compute start addresses for CPU and GPU handles
		handleCPU.InitOffsetted(m_dsvPool->GetCPUDescriptorHandleForHeapStart(), m_numDsvs, m_strideDsv);
		*handle = handleCPU;

		// Create a descriptor
		for (auto i = 0u; i < numDescriptors; ++i)
		{
			const auto& descriptor = descriptors[i];

			m_device->CreateDepthStencilView(descriptor->pResource.Get(), &descriptor->Dsv, handleCPU);
			handleCPU.Offset(m_strideDsv);
			++m_numDsvs;
		}

		return handle;
	}

	return nullptr;
}

DepthStencilHandle DescriptorTablePool::getDepthStencilHandle(const std::string & key)
{
	if (key.size() > 0)
	{
		const auto handleIter = m_dsvHandles.find(key);

		// Create one, if it does not exist
		if (handleIter == m_dsvHandles.end())
		{
			reallocateDescriptorPool(key);

			const auto handle = createDepthStencilHandle(key);
			m_dsvHandles[key] = handle;

			return handle;
		}

		return handleIter->second;
	}

	return nullptr;
}
