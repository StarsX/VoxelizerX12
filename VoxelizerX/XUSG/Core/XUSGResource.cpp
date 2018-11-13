//--------------------------------------------------------------------------------------
// By Stars XU Tianchen
//--------------------------------------------------------------------------------------

#include "DXFrameworkHelper.h"
#include "XUSGResource.h"

#define	REMOVE_PACKED_UAV	ResourceFlags(~0x8000)

using namespace std;
using namespace XUSG;

Format MapToPackedFormat(Format &format)
{
	auto formatUAV = DXGI_FORMAT_R32_UINT;

	switch (format)
	{
	case DXGI_FORMAT_R10G10B10A2_UNORM:
	case DXGI_FORMAT_R10G10B10A2_UINT:
		format = DXGI_FORMAT_R10G10B10A2_TYPELESS;
		break;
	case DXGI_FORMAT_R8G8B8A8_UNORM:
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
	case DXGI_FORMAT_R8G8B8A8_UINT:
	case DXGI_FORMAT_R8G8B8A8_SNORM:
	case DXGI_FORMAT_R8G8B8A8_SINT:
		format = DXGI_FORMAT_R8G8B8A8_TYPELESS;
		break;
	case DXGI_FORMAT_B8G8R8A8_UNORM:
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
		format = DXGI_FORMAT_B8G8R8A8_TYPELESS;
		break;
	case DXGI_FORMAT_B8G8R8X8_UNORM:
	case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
		format = DXGI_FORMAT_B8G8R8X8_TYPELESS;
		break;
	case DXGI_FORMAT_R16G16_FLOAT:
	case DXGI_FORMAT_R16G16_UNORM:
	case DXGI_FORMAT_R16G16_UINT:
	case DXGI_FORMAT_R16G16_SNORM:
	case DXGI_FORMAT_R16G16_SINT:
		format = DXGI_FORMAT_R16G16_TYPELESS;
		break;
	default:
		formatUAV = format;
	}

	return formatUAV;
}

//--------------------------------------------------------------------------------------
// Constant buffer
//--------------------------------------------------------------------------------------

ConstantBuffer::ConstantBuffer() :
	m_device(nullptr),
	m_resource(nullptr),
	m_cbvPool(nullptr),
	m_CBV(D3D12_DEFAULT),
	m_pDataBegin(nullptr)
{
}

ConstantBuffer::~ConstantBuffer()
{
}

bool ConstantBuffer::Create(const Device &device, uint32_t byteWidth, uint32_t cbvSize)
{
	M_RETURN(!device, cerr, "The device is NULL.", false);
	m_device = device;

	V_RETURN(m_device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(byteWidth),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_resource)), clog, false);

	// Allocate descriptor pool
	N_RETURN(allocateDescriptorPool(1), false);

	// Describe and create a constant buffer view.
	D3D12_CONSTANT_BUFFER_VIEW_DESC desc;
	desc.BufferLocation = m_resource->GetGPUVirtualAddress();
	desc.SizeInBytes = (cbvSize + 255) & ~255;	// CB size is required to be 256-byte aligned.

	// Create CBV
	m_CBV = m_cbvPool->GetCPUDescriptorHandleForHeapStart();
	m_device->CreateConstantBufferView(&desc, m_CBV);

	return true;
}

void *ConstantBuffer::Map()
{
	if (m_pDataBegin == nullptr)
	{
		// Map and initialize the constant buffer. We don't unmap this until the
		// app closes. Keeping things mapped for the lifetime of the resource is okay.
		CD3DX12_RANGE readRange(0, 0);	// We do not intend to read from this resource on the CPU.
		V_RETURN(m_resource->Map(0, &readRange, &m_pDataBegin), cerr, false);
	}

	return m_pDataBegin;
}

void ConstantBuffer::Unmap()
{
	m_resource->Unmap(0, nullptr);
	m_pDataBegin = nullptr;
}

const Resource &ConstantBuffer::GetResource() const
{
	return m_resource;
}

const Descriptor &ConstantBuffer::GetCBV() const
{
	return m_CBV;
}

bool ConstantBuffer::allocateDescriptorPool(uint32_t numDescriptors)
{
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = numDescriptors;
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	V_RETURN(m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_cbvPool)), cerr, false);

	return true;
}

//--------------------------------------------------------------------------------------
// Resource base
//--------------------------------------------------------------------------------------

ResourceBase::ResourceBase() :
	m_device(nullptr),
	m_resource(nullptr),
	m_srvUavPool(nullptr),
	m_SRV(D3D12_DEFAULT),
	m_srvUavCurrent(D3D12_DEFAULT),
	m_state(ResourceState(0))
{
}

ResourceBase::~ResourceBase()
{
}

void ResourceBase::Barrier(const GraphicsCommandList &commandList, ResourceState dstState)
{
	if (m_state != dstState)
		commandList->ResourceBarrier(1, &Transition(dstState));
}

const Resource &ResourceBase::GetResource() const
{
	return m_resource;
}

const Descriptor &ResourceBase::GetSRV() const
{
	return m_SRV;
}

ResourceBarrier ResourceBase::Transition(ResourceState dstState)
{
	const auto srcState = m_state;
	m_state = dstState;

	return CD3DX12_RESOURCE_BARRIER::Transition(m_resource.Get(), srcState, dstState);
}

void ResourceBase::setDevice(const Device & device)
{
	m_device = device;

	if (m_device)
		m_strideSrvUav = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

bool ResourceBase::allocateDescriptorPool(uint32_t numDescriptors)
{
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = numDescriptors;
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	V_RETURN(m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_srvUavPool)), cerr, false);

	m_srvUavCurrent = m_srvUavPool->GetCPUDescriptorHandleForHeapStart();

	return true;
}

//--------------------------------------------------------------------------------------
// 2D Texture
//--------------------------------------------------------------------------------------

Texture2D::Texture2D() :
	ResourceBase(),
	m_counter(nullptr),
	m_UAVs(0),
	m_SRVs(0),
	m_subSRVs(0)
{
}

Texture2D::~Texture2D()
{
}

bool Texture2D::Create(const Device &device, uint32_t width, uint32_t height, Format format,
	uint32_t arraySize, ResourceFlags resourceFlags, uint8_t numMips, uint8_t sampleCount,
	PoolType poolType, ResourceState state)
{
	M_RETURN(!device, cerr, "The device is NULL.", false);
	setDevice(device);

	const auto isPacked = static_cast<bool>(resourceFlags & BIND_PACKED_UAV);
	resourceFlags &= REMOVE_PACKED_UAV;

	const auto hasSRV = !(resourceFlags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE);
	const auto hasUAV = resourceFlags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	// Map formats
	auto formatReousrce = format;
	const auto formatUAV = isPacked ? MapToPackedFormat(formatReousrce) : format;

	// Setup the texture description.
	const auto desc = CD3DX12_RESOURCE_DESC::Tex2D(formatReousrce, width, height, arraySize,
		numMips, sampleCount, 0, D3D12_RESOURCE_FLAGS(resourceFlags));

	// Determine initial state
	if (state) m_state = state;
	else
	{
		m_state = hasSRV ? D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE |
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_COMMON;
		m_state = hasUAV ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS : m_state;
	}

	V_RETURN(m_device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(poolType),
		D3D12_HEAP_FLAG_NONE, &desc, m_state, nullptr, IID_PPV_ARGS(&m_resource)), clog, false);

	// Allocate descriptor pool
	auto numDescriptors = hasSRV ? 1 : 0u;
	numDescriptors += hasUAV ? numMips : 0;
	numDescriptors += hasSRV && hasUAV && numMips > 1 ? numMips : 0;
	numDescriptors += hasSRV && hasUAV ? (max)(numMips, 1ui8) - 1 : 0;	// Sub SRVs
	N_RETURN(allocateDescriptorPool(numDescriptors), false);

	// Create SRV
	if (hasSRV) CreateSRV(arraySize, format, numMips, sampleCount);

	// Create UAVs
	if (hasUAV) CreateUAVs(arraySize, formatUAV, numMips);

	// Create SRV for each level
	if (hasSRV && hasUAV) CreateSRVs(arraySize, numMips, format, sampleCount);

	return true;
}

bool Texture2D::Upload(const GraphicsCommandList &commandList, Resource &resourceUpload,
	SubresourceData *pSubresourceData, uint32_t numSubresources, ResourceState dstState)
{
	N_RETURN(pSubresourceData, false);

	const auto uploadBufferSize = GetRequiredIntermediateSize(m_resource.Get(), 0, numSubresources);

	// Create the GPU upload buffer.
	V_RETURN(m_device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&resourceUpload)), clog, false);

	// Copy data to the intermediate upload heap and then schedule a copy 
	// from the upload heap to the Texture2D.
	dstState = dstState ? dstState : m_state;
	if (m_state != D3D12_RESOURCE_STATE_COPY_DEST) Barrier(commandList, D3D12_RESOURCE_STATE_COPY_DEST);
	M_RETURN(UpdateSubresources(commandList.Get(), m_resource.Get(), resourceUpload.Get(),
		0, 0, numSubresources, pSubresourceData) <= 0, clog, "Failed to upload the resource.", false);
	Barrier(commandList, dstState);

	return true;
}

bool Texture2D::Upload(const GraphicsCommandList &commandList, Resource &resourceUpload,
	const uint8_t *pData, uint8_t stride, ResourceState dstState)
{
	const auto desc = m_resource->GetDesc();

	SubresourceData subresourceData;
	subresourceData.pData = pData;
	subresourceData.RowPitch = stride * static_cast<uint32_t>(desc.Width);
	subresourceData.SlicePitch = subresourceData.RowPitch * desc.Height;

	return Upload(commandList, resourceUpload, &subresourceData, 1, dstState);
}

void Texture2D::CreateSRV(uint32_t arraySize, Format format, uint8_t numMips, uint8_t sampleCount)
{
	// Setup the description of the shader resource view.
	D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
	desc.Format = format ? format : m_resource->GetDesc().Format;
	desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	if (arraySize > 1)
	{
		if (sampleCount > 1)
		{
			desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
			desc.Texture2DMSArray.ArraySize = arraySize;
		}
		else
		{
			desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
			desc.Texture2DArray.ArraySize = arraySize;
			desc.Texture2DArray.MipLevels = numMips;
		}
	}
	else
	{
		if (sampleCount > 1)
			desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
		else
		{
			desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			desc.Texture2D.MipLevels = numMips;
		}
	}

	// Create a shader resource view
	m_SRV = m_srvUavCurrent;
	m_device->CreateShaderResourceView(m_resource.Get(), &desc, m_SRV);
	m_srvUavCurrent.Offset(m_strideSrvUav);
}

void Texture2D::CreateSRVs(uint32_t arraySize, uint8_t numMips, Format format, uint8_t sampleCount)
{
	if (numMips > 1)
	{
		// Setup the description of the shader resource view.
		D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
		desc.Format = format ? format : m_resource->GetDesc().Format;
		desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

		auto mipLevel = 0ui8;
		m_SRVs.resize(numMips);

		for (auto &descriptor : m_SRVs)
		{
			// Setup the description of the shader resource view.
			if (arraySize > 1)
			{
				desc.Texture2DArray.MostDetailedMip = mipLevel++;
				desc.Texture2DArray.MipLevels = 1;
			}
			else
			{
				desc.Texture2D.MostDetailedMip = mipLevel++;
				desc.Texture2D.MipLevels = 1;
			}

			descriptor = m_srvUavCurrent;
			m_device->CreateShaderResourceView(m_resource.Get(), &desc, descriptor);
			m_srvUavCurrent.Offset(m_strideSrvUav);
		}
	}
}

void Texture2D::CreateUAVs(uint32_t arraySize, Format format, uint8_t numMips)
{
	format = format ? format : m_resource->GetDesc().Format;

	auto mipLevel = 0ui8;
	m_UAVs.resize(numMips);
	
	for (auto &descriptor : m_UAVs)
	{
		// Setup the description of the unordered access view.
		D3D12_UNORDERED_ACCESS_VIEW_DESC desc = {};
		desc.Format = format;

		if (arraySize > 1)
		{
			desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
			desc.Texture2DArray.ArraySize = arraySize;
			desc.Texture2DArray.MipSlice = mipLevel++;
		}
		else
		{
			desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			desc.Texture2D.MipSlice = mipLevel++;
		}

		// Create an unordered access view
		descriptor = m_srvUavCurrent;
		m_device->CreateUnorderedAccessView(m_resource.Get(), m_counter.Get(), &desc, descriptor);
		m_srvUavCurrent.Offset(m_strideSrvUav);
	}
}

void Texture2D::CreateSubSRVs(Format format)
{
	const auto txDesc = m_resource->GetDesc();
	format = format ? format : txDesc.Format;

	auto mipLevel = 1ui8;
	const auto numLevels = (max)(txDesc.MipLevels, 1ui16) - 1;
	m_subSRVs.resize(numLevels);

	for (auto &descriptor : m_subSRVs)
	{
		// Setup the description of the shader resource view.
		D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
		desc.Format = format;
		desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		
		if (txDesc.DepthOrArraySize > 1)
		{
			desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
			desc.Texture2DArray.ArraySize = txDesc.DepthOrArraySize;
			desc.Texture2DArray.MostDetailedMip = mipLevel;
			desc.Texture2DArray.MipLevels = txDesc.MipLevels - mipLevel;
		}
		else
		{
			desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			desc.Texture2D.MostDetailedMip = mipLevel;
			desc.Texture2D.MipLevels = txDesc.MipLevels - mipLevel;
		}
		++mipLevel;

		// Create a shader resource view
		descriptor = m_srvUavCurrent;
		m_device->CreateShaderResourceView(m_resource.Get(), &desc, descriptor);
		m_srvUavCurrent.Offset(m_strideSrvUav);
	}
}

Descriptor Texture2D::GetUAV(uint8_t i) const
{
	return m_UAVs.size() > i ? m_UAVs[i] : Descriptor(D3D12_DEFAULT);
}

Descriptor Texture2D::GetSRVAtLevel(uint8_t i) const
{
	return m_SRVs.size() > i ? m_SRVs[i] : Descriptor(D3D12_DEFAULT);
}

Descriptor Texture2D::GetSubSRV(uint8_t i) const
{
	return m_subSRVs.size() > i ? m_subSRVs[i] : Descriptor(D3D12_DEFAULT);
}

//--------------------------------------------------------------------------------------
// Render target
//--------------------------------------------------------------------------------------

RenderTarget::RenderTarget() :
	Texture2D(),
	m_rtvPool(nullptr),
	m_RTVs(0),
	m_rtvCurrent(D3D12_DEFAULT)
{
}

RenderTarget::~RenderTarget()
{
}

bool RenderTarget::Create(const Device &device, uint32_t width, uint32_t height, Format format,
	uint32_t arraySize, ResourceFlags resourceFlags, uint8_t numMips, uint8_t sampleCount,
	ResourceState state)
{
	N_RETURN(create(device, width, height, arraySize, format, numMips,
		sampleCount, resourceFlags, state), false);

	numMips = (max)(numMips, 1ui8);
	N_RETURN(allocateRtvPool(numMips * arraySize), false);

	m_RTVs.resize(arraySize);
	for (auto i = 0u; i < arraySize; ++i)
	{
		auto mipLevel = 0ui8;
		m_RTVs[i].resize(numMips);

		for (auto &descriptor : m_RTVs[i])
		{
			// Setup the description of the render target view.
			D3D12_RENDER_TARGET_VIEW_DESC desc = {};
			desc.Format = format;

			if (arraySize > 1)
			{
				if (sampleCount > 1)
				{
					desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
					desc.Texture2DMSArray.FirstArraySlice = i;
					desc.Texture2DMSArray.ArraySize = 1;
				}
				else
				{
					desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
					desc.Texture2DArray.FirstArraySlice = i;
					desc.Texture2DArray.ArraySize = 1;
					desc.Texture2DArray.MipSlice = mipLevel++;
				}
			}
			else
			{
				if (sampleCount > 1)
					desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
				else
				{
					desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
					desc.Texture2D.MipSlice = mipLevel++;
				}
			}

			// Create a render target view
			descriptor = m_rtvCurrent;
			m_device->CreateRenderTargetView(m_resource.Get(), &desc, descriptor);
			m_rtvCurrent.Offset(m_strideRtv);
		}
	}

	return true;
}

bool RenderTarget::CreateArray(const Device &device, uint32_t width, uint32_t height,
	uint32_t arraySize, Format format, ResourceFlags resourceFlags, uint8_t numMips,
	uint8_t sampleCount, ResourceState state)
{
	N_RETURN(create(device, width, height, arraySize, format, numMips,
		sampleCount, resourceFlags, state), false);

	numMips = (max)(numMips, 1ui8);
	N_RETURN(allocateRtvPool(numMips), false);

	m_RTVs.resize(1);
	m_RTVs[0].resize(numMips);

	auto mipLevel = 0ui8;
	for (auto &descriptor : m_RTVs[0])
	{
		// Setup the description of the render target view.
		D3D12_RENDER_TARGET_VIEW_DESC desc = {};
		desc.Format = format;

		if (sampleCount > 1)
		{
			desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
			desc.Texture2DMSArray.ArraySize = arraySize;
		}
		else
		{
			desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
			desc.Texture2DArray.ArraySize = arraySize;
			desc.Texture2DArray.MipSlice = mipLevel++;
		}

		// Create a render target view
		descriptor = m_rtvCurrent;
		m_device->CreateRenderTargetView(m_resource.Get(), &desc, descriptor);
		m_rtvCurrent.Offset(m_strideRtv);
	}

	return true;
}

Descriptor RenderTarget::GetRTV(uint32_t slice, uint8_t mipLevel) const
{
	return m_RTVs.size() > slice && m_RTVs[slice].size() > mipLevel ?
		m_RTVs[slice][mipLevel] : Descriptor(D3D12_DEFAULT);
}

uint32_t RenderTarget::GetArraySize() const
{
	return static_cast<uint32_t>(m_RTVs.size());
}

uint8_t RenderTarget::GetNumMips(uint32_t slice) const
{
	return m_RTVs.size() > slice ? static_cast<uint8_t>(m_RTVs[slice].size()) : 0;
}

bool RenderTarget::create(const Device &device, uint32_t width, uint32_t height,
	uint32_t arraySize, Format format, uint8_t numMips, uint8_t sampleCount,
	ResourceFlags resourceFlags, ResourceState state)
{
	M_RETURN(!device, cerr, "The device is NULL.", false);
	setDevice(device);

	m_strideRtv = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	const auto isPacked = static_cast<bool>(resourceFlags & BIND_PACKED_UAV);
	resourceFlags &= REMOVE_PACKED_UAV;

	const auto hasSRV = !(resourceFlags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE);
	const auto hasUAV = resourceFlags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	// Map formats
	auto formatResource = format;
	const auto formatUAV = isPacked ? MapToPackedFormat(formatResource) : format;

	// Setup the texture description.
	const auto desc = CD3DX12_RESOURCE_DESC::Tex2D(formatResource, width, height, arraySize,
		numMips, sampleCount, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | resourceFlags);

	// Determine initial state
	m_state = state ? state : D3D12_RESOURCE_STATE_RENDER_TARGET;

	// Create the render target texture.
	V_RETURN(m_device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE, &desc, m_state, nullptr, IID_PPV_ARGS(&m_resource)), clog, false);
	
	// Allocate descriptor pool
	auto numDescriptors = hasSRV ? 1 + (numMips > 1 ? numMips : 0) : 0u;
	numDescriptors += hasUAV ? numMips : 0;
	numDescriptors += hasSRV ? (max)(numMips, 1ui8) - 1 : 0;	// Sub SRVs
	N_RETURN(allocateDescriptorPool(numDescriptors), false);

	// Create SRV
	if (hasSRV) CreateSRV(arraySize, format, numMips, sampleCount);

	// Create UAV
	if (hasUAV) CreateUAVs(arraySize, formatUAV, numMips);

	return true;
}

bool RenderTarget::allocateRtvPool(uint32_t numDescriptors)
{
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = numDescriptors;
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	V_RETURN(m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_rtvPool)), cerr, false);
	
	m_rtvCurrent = m_rtvPool->GetCPUDescriptorHandleForHeapStart();

	return true;
}

//--------------------------------------------------------------------------------------
// Depth stencil
//--------------------------------------------------------------------------------------

DepthStencil::DepthStencil() :
	Texture2D(),
	m_dsvPool(nullptr),
	m_DSVs(0),
	m_DSVROs(0),
	m_SRVStencil(D3D12_DEFAULT),
	m_dsvCurrent(D3D12_DEFAULT)
{
}

DepthStencil::~DepthStencil()
{
}

bool DepthStencil::Create(const Device &device, uint32_t width, uint32_t height, Format format,
	ResourceFlags resourceFlags, uint32_t arraySize, uint8_t numMips, uint8_t sampleCount,
	ResourceState state, uint8_t clearStencil, float clearDepth)
{
	M_RETURN(!device, cerr, "The device is NULL.", false);
	setDevice(device);
	
	m_strideDsv = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	const auto hasSRV = !(resourceFlags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE);

	// Map formats
	auto formatResource = format;
	auto formatDepth = DXGI_FORMAT_UNKNOWN;
	auto formatStencil = DXGI_FORMAT_UNKNOWN;

	if (hasSRV)
	{
		switch (format)
		{
		case DXGI_FORMAT_D24_UNORM_S8_UINT:
			formatResource = DXGI_FORMAT_R24G8_TYPELESS;
			formatDepth = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
			formatStencil = DXGI_FORMAT_X24_TYPELESS_G8_UINT;
			break;
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
			formatResource = DXGI_FORMAT_R32G8X24_TYPELESS;
			formatDepth = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
			formatStencil = DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;
			break;
		case DXGI_FORMAT_D16_UNORM:
			formatResource = DXGI_FORMAT_R16_TYPELESS;
			formatDepth = DXGI_FORMAT_R16_UNORM;
			break;
		case DXGI_FORMAT_D32_FLOAT:
			formatResource = DXGI_FORMAT_R32_TYPELESS;
			formatDepth = DXGI_FORMAT_R32_FLOAT;
		default:
			formatResource = DXGI_FORMAT_R24G8_TYPELESS;
			formatDepth = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		}
	}

	// Setup the render depth stencil description.
	{
		const auto desc = CD3DX12_RESOURCE_DESC::Tex2D(formatResource, width, height, arraySize,
			numMips, sampleCount, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | resourceFlags);

		// Determine initial state
		m_state = state ? state : D3D12_RESOURCE_STATE_DEPTH_WRITE;

		// Optimized clear value
		D3D12_CLEAR_VALUE clearValue;
		clearValue.Format = format;
		clearValue.DepthStencil.Depth = clearDepth;
		clearValue.DepthStencil.Stencil = clearStencil;

		// Create the depth stencil texture.
		V_RETURN(m_device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE, &desc, m_state, &clearValue, IID_PPV_ARGS(&m_resource)), clog, false);
	}

	// Allocate descriptor pool
	auto numDescriptors = hasSRV ? 1 + (numMips > 1 ? numMips : 0) : 0u;
	numDescriptors += formatStencil ? 1 : 0;			// Stencil SRV
	numDescriptors += hasSRV ? (max)(numMips, 1ui8) - 1 : 0;	// Sub SRVs
	N_RETURN(allocateDescriptorPool(numDescriptors), false);

	if (hasSRV)
	{
		// Create SRV
		if (hasSRV)
			CreateSRV(arraySize, formatDepth, numMips, sampleCount);

		// Has stencil
		if (formatStencil)
		{
			// Setup the description of the shader resource view.
			D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
			desc.Format = format;
			desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

			if (arraySize > 1)
			{
				if (sampleCount > 1)
				{
					desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
					desc.Texture2DMSArray.ArraySize = arraySize;
				}
				else
				{
					desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
					desc.Texture2DArray.ArraySize = arraySize;
					desc.Texture2DArray.MipLevels = numMips;
				}
			}
			else
			{
				if (sampleCount > 1)
					desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
				else
				{
					desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
					desc.Texture2D.MipLevels = numMips;
				}
			}

			// Create a shader resource view
			m_SRVStencil = m_srvUavCurrent;
			m_device->CreateShaderResourceView(m_resource.Get(), &desc, m_SRVStencil);
			m_srvUavCurrent.Offset(m_strideSrvUav);
		}
	}

	numMips = (max)(numMips, 1ui8);
	N_RETURN(allocateDsvPool(hasSRV ? numMips * 2 : numMips), false);

	m_DSVs.resize(numMips);
	m_DSVROs.resize(numMips);

	for (auto i = 0ui8; i < numMips; ++i)
	{
		// Setup the description of the depth stencil view.
		D3D12_DEPTH_STENCIL_VIEW_DESC desc = {};
		desc.Format = format;

		if (arraySize > 1)
		{
			if (sampleCount > 1)
			{
				desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
				desc.Texture2DMSArray.ArraySize = arraySize;
			}
			else
			{
				desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
				desc.Texture2DArray.ArraySize = arraySize;
				desc.Texture2DArray.MipSlice = i;
			}
		}
		else
		{
			if (sampleCount > 1)
				desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
			else
			{
				desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
				desc.Texture2D.MipSlice = i;
			}
		}

		// Create a depth stencil view
		m_DSVs[i] = m_dsvCurrent;
		m_device->CreateDepthStencilView(m_resource.Get(), &desc, m_DSVs[i]);
		m_dsvCurrent.Offset(m_strideDsv);

		// Read-only depth stencil
		if (hasSRV)
		{
			// Setup the description of the depth stencil view.
			desc.Flags = formatStencil ? D3D12_DSV_FLAG_READ_ONLY_DEPTH |
				D3D12_DSV_FLAG_READ_ONLY_STENCIL : D3D12_DSV_FLAG_READ_ONLY_DEPTH;

			// Create a depth stencil view
			m_DSVROs[i] = m_dsvCurrent;
			m_device->CreateDepthStencilView(m_resource.Get(), &desc, m_DSVROs[i]);
			m_dsvCurrent.Offset(m_strideDsv);
		}
		else m_DSVROs[i] = m_DSVs[i];
	}

	return true;
}

Descriptor DepthStencil::GetDSV(uint8_t mipLevel) const
{
	return m_DSVs.size() > mipLevel ? m_DSVs[mipLevel] : Descriptor(D3D12_DEFAULT);
}

Descriptor DepthStencil::GetDSVReadOnly(uint8_t mipLevel) const
{
	return m_DSVROs.size() > mipLevel ? m_DSVROs[mipLevel] : Descriptor(D3D12_DEFAULT);
}

const Descriptor &DepthStencil::GetSRVStencil() const
{
	return m_SRVStencil;
}

const uint8_t DepthStencil::GetNumMips() const
{
	return static_cast<uint8_t>(m_DSVs.size());
}

bool DepthStencil::allocateDsvPool(uint32_t numDescriptors)
{
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = numDescriptors;
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	V_RETURN(m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_dsvPool)), cerr, false);

	m_dsvCurrent = m_dsvPool->GetCPUDescriptorHandleForHeapStart();

	return true;
}

//--------------------------------------------------------------------------------------
// 3D Texture
//--------------------------------------------------------------------------------------

Texture3D::Texture3D() :
	ResourceBase(),
	m_counter(nullptr),
	m_UAVs(0),
	m_SRVs(0),
	m_subSRVs(0)
{
}

Texture3D::~Texture3D()
{
}

bool Texture3D::Create(const Device &device, uint32_t width, uint32_t height,
	uint32_t depth, Format format, ResourceFlags resourceFlags, uint8_t numMips,
	PoolType poolType, ResourceState state)
{
	M_RETURN(!device, cerr, "The device is NULL.", false);
	setDevice(device);

	const auto isPacked = static_cast<bool>(resourceFlags & BIND_PACKED_UAV);
	resourceFlags &= REMOVE_PACKED_UAV;

	const auto hasSRV = !(resourceFlags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE);
	const auto hasUAV = resourceFlags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	// Map formats
	auto formatReousrce = format;
	const auto formatUAV = isPacked ? MapToPackedFormat(formatReousrce) : format;

	// Setup the texture description.
	const auto desc = CD3DX12_RESOURCE_DESC::Tex3D(formatReousrce, width, height, depth,
		numMips, D3D12_RESOURCE_FLAGS(resourceFlags));

	// Determine initial state
	if (state) m_state = state;
	else
	{
		m_state = hasSRV ? D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE |
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_COMMON;
		m_state = hasUAV ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS : m_state;
	}

	V_RETURN(m_device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(poolType),
		D3D12_HEAP_FLAG_NONE, &desc, m_state, nullptr, IID_PPV_ARGS(&m_resource)), clog, false);

	// Allocate descriptor pool
	auto numDescriptors = hasSRV ? 1 : 0u;
	numDescriptors += hasUAV ? numMips : 0;
	numDescriptors += hasSRV && hasUAV && numMips > 1 ? numMips : 0;
	numDescriptors += hasSRV && hasUAV ? (max)(numMips, 1ui8) - 1 : 0;	// Sub SRVs
	N_RETURN(allocateDescriptorPool(numDescriptors), false);

	// Create SRV
	if (hasSRV) CreateSRV(format, numMips);

	// Create UAV
	if (hasUAV) CreateUAVs(formatUAV, numMips);

	// Create SRV for each level
	if (hasSRV && hasUAV) CreateSRVs(numMips, format);

	return true;
}

void Texture3D::CreateSRV(Format format, uint8_t numMips)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
	desc.Format = format ? format : m_resource->GetDesc().Format;
	desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
	desc.Texture3D.MipLevels = numMips;

	// Create a shader resource view
	m_SRV = m_srvUavCurrent;
	m_device->CreateShaderResourceView(m_resource.Get(), &desc, m_SRV);
	m_srvUavCurrent.Offset(m_strideSrvUav);
}

void Texture3D::CreateSRVs(uint8_t numMips, Format format)
{
	if (numMips > 1)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
		desc.Format = format ? format : m_resource->GetDesc().Format;
		desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;

		auto mipLevel = 0ui8;
		m_SRVs.resize(numMips);

		for (auto &descriptor : m_SRVs)
		{
			// Setup the description of the shader resource view.
			desc.Texture3D.MostDetailedMip = mipLevel++;
			desc.Texture3D.MipLevels = 1;

			// Create a shader resource view
			descriptor = m_srvUavCurrent;
			m_device->CreateShaderResourceView(m_resource.Get(), &desc, descriptor);
			m_srvUavCurrent.Offset(m_strideSrvUav);
		}
	}
}

void Texture3D::CreateUAVs(Format format, uint8_t numMips)
{
	const auto txDesc = m_resource->GetDesc();
	format = format ? format : txDesc.Format;

	auto mipLevel = 0ui8;
	m_UAVs.resize(numMips);

	for (auto &descriptor : m_UAVs)
	{
		// Setup the description of the unordered access view.
		D3D12_UNORDERED_ACCESS_VIEW_DESC desc = {};
		desc.Format = format;
		desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
		desc.Texture3D.WSize = txDesc.DepthOrArraySize >> mipLevel;
		desc.Texture3D.MipSlice = mipLevel++;

		// Create an unordered access view
		descriptor = m_srvUavCurrent;
		m_device->CreateUnorderedAccessView(m_resource.Get(), m_counter.Get(), &desc, descriptor);
		m_srvUavCurrent.Offset(m_strideSrvUav);
	}
}

void Texture3D::CreateSubSRVs(Format format)
{
	const auto txDesc = m_resource->GetDesc();
	format = format ? format : txDesc.Format;

	auto mipLevel = 1ui8;
	m_subSRVs.resize((max)(txDesc.MipLevels, 1ui16) - 1);

	for (auto &descriptor : m_subSRVs)
	{
		// Setup the description of the shader resource view.
		D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
		desc.Format = format;
		desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
		desc.Texture3D.MostDetailedMip = mipLevel;
		desc.Texture3D.MipLevels = txDesc.MipLevels - mipLevel;
		++mipLevel;

		// Create a shader resource view
		descriptor = m_srvUavCurrent;
		m_device->CreateShaderResourceView(m_resource.Get(), &desc, descriptor);
		m_srvUavCurrent.Offset(m_strideSrvUav);
	}
}

Descriptor Texture3D::GetUAV(uint8_t i) const
{
	return m_UAVs.size() > i ? m_UAVs[i] : Descriptor(D3D12_DEFAULT);
}

Descriptor Texture3D::GetSRVLevel(uint8_t i) const
{
	return m_SRVs.size() > i ? m_SRVs[i] : Descriptor(D3D12_DEFAULT);
}

Descriptor Texture3D::GetSubSRV(uint8_t i) const
{
	return m_subSRVs.size() > i ? m_subSRVs[i] : Descriptor(D3D12_DEFAULT);
}

//--------------------------------------------------------------------------------------
// Buffer base
//--------------------------------------------------------------------------------------

BufferBase::BufferBase() :
	ResourceBase()
{
}

BufferBase::~BufferBase()
{
}

bool BufferBase::Upload(const GraphicsCommandList &commandList, Resource &resourceUpload,
	const void *pData, ResourceState dstState)
{
	const auto desc = m_resource->GetDesc();
	const auto uploadBufferSize = GetRequiredIntermediateSize(m_resource.Get(), 0, 1);

	// Create the GPU upload buffer.
	V_RETURN(m_device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&resourceUpload)), clog, false);

	// Copy data to the intermediate upload heap and then schedule a copy 
	// from the upload heap to the buffer.
	D3D12_SUBRESOURCE_DATA subresourceData;
	subresourceData.pData = pData;
	subresourceData.RowPitch = static_cast<uint32_t>(uploadBufferSize);
	subresourceData.SlicePitch = subresourceData.RowPitch;

	dstState = dstState ? dstState : m_state;
	if (m_state != D3D12_RESOURCE_STATE_COPY_DEST) Barrier(commandList, D3D12_RESOURCE_STATE_COPY_DEST);
	M_RETURN(UpdateSubresources(commandList.Get(), m_resource.Get(), resourceUpload.Get(),
		0, 0, 1, &subresourceData) <= 0, clog, "Failed to upload the resource.", false);
	Barrier(commandList, dstState);

	return true;
}

//--------------------------------------------------------------------------------------
// Raw buffer
//--------------------------------------------------------------------------------------

RawBuffer::RawBuffer() :
	BufferBase(),
	m_counter(nullptr),
	m_UAV(D3D12_DEFAULT)
{
}

RawBuffer::~RawBuffer()
{
}

bool RawBuffer::Create(const Device &device, uint32_t byteWidth, ResourceFlags resourceFlags,
	PoolType poolType, ResourceState state)
{
	const auto hasSRV = !(resourceFlags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE);
	const auto hasUAV = resourceFlags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	// Create buffer
	N_RETURN(create(device, byteWidth, resourceFlags, poolType, state, hasSRV, hasUAV), false);

	// Create SRV
	if (hasSRV) CreateSRV(byteWidth);

	// Create UAV
	if (hasUAV) CreateUAV(byteWidth);

	return true;
}

void RawBuffer::CreateSRV(uint32_t byteWidth)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
	desc.Format = DXGI_FORMAT_R32_TYPELESS;
	desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	desc.Buffer.NumElements = byteWidth / 4;
	desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;

	// Create a shader resource view
	m_SRV = m_srvUavCurrent;
	m_device->CreateShaderResourceView(m_resource.Get(), &desc, m_SRV);
	m_srvUavCurrent.Offset(m_strideSrvUav);
}

void RawBuffer::CreateUAV(uint32_t byteWidth)
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC desc = {};
	desc.Format = DXGI_FORMAT_R32_TYPELESS;
	desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	desc.Buffer.NumElements = byteWidth / 4;
	desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;

	// Create an unordered access view
	m_UAV = m_srvUavCurrent;
	m_device->CreateUnorderedAccessView(m_resource.Get(), m_counter.Get(), &desc, m_UAV);
	m_srvUavCurrent.Offset(m_strideSrvUav);
}

const Descriptor &RawBuffer::GetUAV() const
{
	return m_UAV;
}

bool RawBuffer::create(const Device &device, uint32_t byteWidth, ResourceFlags resourceFlags,
	PoolType poolType, ResourceState state, bool hasSRV, bool hasUAV)
{
	M_RETURN(!device, cerr, "The device is NULL.", false);
	setDevice(device);

	// Setup the buffer description.
	const auto desc = CD3DX12_RESOURCE_DESC::Buffer(byteWidth, D3D12_RESOURCE_FLAGS(resourceFlags));

	// Determine initial state
	if (state) m_state = state;
	else
	{
		m_state = hasSRV ? D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE |
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_COMMON;
		m_state = hasUAV ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS : m_state;
	}

	V_RETURN(m_device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(poolType),
		D3D12_HEAP_FLAG_NONE, &desc, m_state, nullptr, IID_PPV_ARGS(&m_resource)), clog, false);

	// Allocate descriptor pool
	auto numDescriptors = hasSRV ? 1 : 0u;
	numDescriptors += hasUAV ? 1 : 0;
	N_RETURN(allocateDescriptorPool(numDescriptors), false);

	return true;
}

//--------------------------------------------------------------------------------------
// Vertex buffer
//--------------------------------------------------------------------------------------

VertexBuffer::VertexBuffer() :
	RawBuffer(),
	m_VBV()
{
}

VertexBuffer::~VertexBuffer()
{
}

bool VertexBuffer::Create(const Device &device, uint32_t byteWidth, uint32_t stride,
	ResourceFlags resourceFlags, PoolType poolType, ResourceState state)
{
	const auto hasSRV = !(resourceFlags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE);
	const auto hasUAV = resourceFlags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	// Determine initial state
	if (state == 0)
	{
		state = hasSRV ? D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE :
			D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		state = hasUAV ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS : state;
	}

	N_RETURN(RawBuffer::Create(device, byteWidth, resourceFlags, poolType, state), false);

	// Create vertex buffer view
	m_VBV.BufferLocation = m_resource->GetGPUVirtualAddress();
	m_VBV.StrideInBytes = stride;
	m_VBV.SizeInBytes = byteWidth;

	return true;
}

const VertexBufferView &VertexBuffer::GetVBV() const
{
	return m_VBV;
}

//--------------------------------------------------------------------------------------
// Index buffer
//--------------------------------------------------------------------------------------

IndexBuffer::IndexBuffer() :
	RawBuffer(),
	m_IBV()
{
}

IndexBuffer::~IndexBuffer()
{
}

bool IndexBuffer::Create(const Device &device, uint32_t byteWidth, Format format,
	ResourceFlags resourceFlags, PoolType poolType, ResourceState state)
{
	setDevice(device);

	const auto hasSRV = !(resourceFlags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE);
	const auto hasUAV = resourceFlags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	assert(format == DXGI_FORMAT_R32_UINT || format == DXGI_FORMAT_R16_UINT);
	if (hasSRV || hasUAV) byteWidth += byteWidth % 4;

	// Determine initial state
	if (state) m_state = state;
	else m_state = hasSRV ? D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE :
		D3D12_RESOURCE_STATE_INDEX_BUFFER;
	
	N_RETURN(RawBuffer::Create(device, byteWidth, resourceFlags, poolType, state), false);

	// Create index buffer view
	m_IBV.BufferLocation = m_resource->GetGPUVirtualAddress();
	m_IBV.SizeInBytes = byteWidth;
	m_IBV.Format = format;

	return true;
}

const IndexBufferView &IndexBuffer::GetIBV() const
{
	return m_IBV;
}

//--------------------------------------------------------------------------------------
// Typed buffer
//--------------------------------------------------------------------------------------

TypedBuffer::TypedBuffer() :
	RawBuffer()
{
}

TypedBuffer::~TypedBuffer()
{
}

bool TypedBuffer::Create(const Device &device, uint32_t numElements, uint32_t stride,
	Format format, ResourceFlags resourceFlags, PoolType poolType, ResourceState state)
{
	M_RETURN(!device, cerr, "The device is NULL.", false);
	setDevice(device);

	const auto isPacked = static_cast<bool>(resourceFlags & BIND_PACKED_UAV);
	resourceFlags &= REMOVE_PACKED_UAV;

	const auto hasSRV = !(resourceFlags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE);
	const auto hasUAV = resourceFlags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	// Map formats
	auto formatReousrce = format;
	const auto formatUAV = isPacked ? MapToPackedFormat(formatReousrce) : format;

	// Setup the buffer description.
	auto desc = CD3DX12_RESOURCE_DESC::Buffer(stride * numElements, D3D12_RESOURCE_FLAGS(resourceFlags));
	desc.Format = formatReousrce;

	// Determine initial state
	if (state) m_state = state;
	else
	{
		m_state = hasSRV ? D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE |
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_COMMON;
		m_state = hasUAV ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS : m_state;
	}

	V_RETURN(m_device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(poolType),
		D3D12_HEAP_FLAG_NONE, &desc, m_state, nullptr, IID_PPV_ARGS(&m_resource)), clog, false);

	// Allocate descriptor pool
	auto numDescriptors = hasSRV ? 1 : 0u;
	numDescriptors += hasUAV ? 1 : 0;
	N_RETURN(allocateDescriptorPool(numDescriptors), false);

	// Create SRV
	if (hasSRV) CreateSRV(numElements, format);

	// Create UAV
	if (hasUAV) CreateUAV(numElements, formatUAV);

	return true;
}

void TypedBuffer::CreateSRV(uint32_t numElements, Format format)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
	desc.Format = format ? format : m_resource->GetDesc().Format;
	desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	desc.Buffer.NumElements = numElements;
	
	// Create a shader resource view
	m_SRV = m_srvUavCurrent;
	m_device->CreateShaderResourceView(m_resource.Get(), &desc, m_SRV);
	m_srvUavCurrent.Offset(m_strideSrvUav);
}

void TypedBuffer::CreateUAV(uint32_t numElements, Format format)
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC desc = {};
	desc.Format = format ? format : m_resource->GetDesc().Format;;
	desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	desc.Buffer.NumElements = numElements;

	// Create an unordered access view
	m_UAV = m_srvUavCurrent;
	m_device->CreateUnorderedAccessView(m_resource.Get(), m_counter.Get(), &desc, m_UAV);
	m_srvUavCurrent.Offset(m_strideSrvUav);
}

//--------------------------------------------------------------------------------------
// Structured buffer
//--------------------------------------------------------------------------------------

StructuredBuffer::StructuredBuffer() :
	RawBuffer()
{
}

StructuredBuffer::~StructuredBuffer()
{
}

bool StructuredBuffer::Create(const Device &device, uint32_t numElements, uint32_t stride,
	ResourceFlags resourceFlags, PoolType poolType, ResourceState state)
{
	const auto hasSRV = !(resourceFlags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE);
	const auto hasUAV = resourceFlags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	// Create buffer
	N_RETURN(create(device, stride * numElements, resourceFlags,
		poolType, state, hasSRV, hasUAV), false);

	// Create SRV
	if (hasSRV) CreateSRV(numElements, stride);

	// Create UAV
	if (hasUAV) CreateUAV(numElements, stride);

	return true;
}

void StructuredBuffer::CreateSRV(uint32_t numElements, uint32_t stride)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
	desc.Format = m_resource->GetDesc().Format;
	desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	desc.Buffer.NumElements = numElements;
	desc.Buffer.StructureByteStride = stride;

	// Create a shader resource view
	m_SRV = m_srvUavCurrent;
	m_device->CreateShaderResourceView(m_resource.Get(), &desc, m_SRV);
	m_srvUavCurrent.Offset(m_strideSrvUav);
}

void StructuredBuffer::CreateUAV(uint32_t numElements, uint32_t stride)
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC desc = {};
	desc.Format = m_resource->GetDesc().Format;
	desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	desc.Buffer.NumElements = numElements;
	desc.Buffer.StructureByteStride = stride;

	// Create an unordered access view
	m_UAV = m_srvUavCurrent;
	m_device->CreateUnorderedAccessView(m_resource.Get(), m_counter.Get(), &desc, m_UAV);
	m_srvUavCurrent.Offset(m_strideSrvUav);
}
