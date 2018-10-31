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

ConstantBuffer::ConstantBuffer(const Device &device) :
	m_device(device),
	m_resource(nullptr),
	m_CBV(nullptr),
	m_pDataBegin(nullptr)
{
}

ConstantBuffer::~ConstantBuffer()
{
}

void ConstantBuffer::Create(uint32_t byteWidth, uint32_t cbvSize)
{
	ThrowIfFailed(m_device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(byteWidth),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_resource)));

	// Describe and create a constant buffer view.
	auto& descriptor = m_CBV;
	descriptor = make_shared<Descriptor>();
	descriptor->ResourceType = Descriptor::CBV;
	descriptor->Cbv.BufferLocation = m_resource->GetGPUVirtualAddress();
	descriptor->Cbv.SizeInBytes = (cbvSize + 255) & ~255;	// CB size is required to be 256-byte aligned.
}

uint8_t *ConstantBuffer::Map()
{
	if (m_pDataBegin == nullptr)
	{
		// Map and initialize the constant buffer. We don't unmap this until the
		// app closes. Keeping things mapped for the lifetime of the resource is okay.
		CD3DX12_RANGE readRange(0, 0);	// We do not intend to read from this resource on the CPU.
		ThrowIfFailed(m_resource->Map(0, &readRange, reinterpret_cast<void**>(&m_pDataBegin)));
	}

	return m_pDataBegin;
}

const Resource& ConstantBuffer::GetResource() const
{
	return m_resource;
}

const DescriptorView& ConstantBuffer::GetCBV() const
{
	return m_CBV;
}

//--------------------------------------------------------------------------------------
// Resource base
//--------------------------------------------------------------------------------------

ResourceBase::ResourceBase(const Device &device) :
	m_device(device),
	m_resource(nullptr),
	m_SRV(nullptr),
	m_state(ResourceState(0))
{
}

ResourceBase::~ResourceBase()
{
}

const Resource& ResourceBase::GetResource() const
{
	return m_resource;
}

const DescriptorView& ResourceBase::GetSRV() const
{
	return m_SRV;
}

ResourceBarrier ResourceBase::Transition(ResourceState dstState)
{
	const auto srcState = m_state;
	m_state = dstState;

	return CD3DX12_RESOURCE_BARRIER::Transition(m_resource.Get(), srcState, dstState);
}

DescriptorView ResourceBase::createSRV(Format format)
{
	const auto descriptor = make_shared<Descriptor>();
	descriptor->ResourceType = Descriptor::SRV;
	descriptor->pResource = m_resource;
	descriptor->Srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	descriptor->Srv.Format = format;

	return descriptor;
}

DescriptorView ResourceBase::createUAV(Format format)
{
	const auto descriptor = make_shared<Descriptor>();
	descriptor->ResourceType = Descriptor::UAV;
	descriptor->pResource = m_resource;
	descriptor->Uav.Format = format;

	return descriptor;
}

//--------------------------------------------------------------------------------------
// 2D Texture
//--------------------------------------------------------------------------------------

Texture2D::Texture2D(const Device &device) :
	ResourceBase(device),
	m_UAVs(0),
	m_SRVs(0),
	m_subSRVs(0)
{
}

Texture2D::~Texture2D()
{
}

void Texture2D::Create(uint32_t width, uint32_t height, Format format, uint32_t arraySize,
	ResourceFlags resourceFlags, uint8_t numMips, uint8_t sampleCount, PoolType poolType,
	ResourceState state)
{
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

	ThrowIfFailed(m_device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(poolType),
		D3D12_HEAP_FLAG_NONE, &desc, m_state, nullptr, IID_PPV_ARGS(&m_resource)));

	// Create SRV
	if (hasSRV)
		CreateSRV(arraySize, format, numMips, sampleCount);

	// Create UAV
	if (hasUAV)
		CreateUAV(arraySize, formatUAV, numMips);
}

void Texture2D::Upload(const GraphicsCommandList& commandList, Resource& resourceUpload,
	const void *pData, uint8_t stride, ResourceState dstState)
{
	const auto desc = m_resource->GetDesc();
	const auto uploadBufferSize = GetRequiredIntermediateSize(m_resource.Get(), 0, 1);

	// Create the GPU upload buffer.
	ThrowIfFailed(m_device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&resourceUpload)));

	// Copy data to the intermediate upload heap and then schedule a copy 
	// from the upload heap to the Texture2D.
	D3D12_SUBRESOURCE_DATA subresourceData;
	subresourceData.pData = pData;
	subresourceData.RowPitch = stride * static_cast<uint32_t>(desc.Width);
	subresourceData.SlicePitch = subresourceData.RowPitch * desc.Height;

	dstState = dstState ? dstState : m_state;
	if (m_state != D3D12_RESOURCE_STATE_COPY_DEST)
		commandList->ResourceBarrier(1, &Transition(D3D12_RESOURCE_STATE_COPY_DEST));
	UpdateSubresources(commandList.Get(), m_resource.Get(), resourceUpload.Get(), 0, 0, 1, &subresourceData);
	commandList->ResourceBarrier(1, &Transition(dstState));
}

void Texture2D::CreateSRV(uint32_t arraySize, Format format, uint8_t numMips, uint8_t sampleCount)
{
	auto& descriptor = m_SRV;
	descriptor = createSRV(format);

	if (arraySize > 1)
	{
		if (sampleCount > 1)
		{
			descriptor->Srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
			descriptor->Srv.Texture2DMSArray.ArraySize = arraySize;
		}
		else
		{
			descriptor->Srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
			descriptor->Srv.Texture2DArray.ArraySize = arraySize;
			descriptor->Srv.Texture2DArray.MipLevels = numMips;
		}
	}
	else
	{
		if (sampleCount > 1)
			descriptor->Srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
		else
		{
			descriptor->Srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			descriptor->Srv.Texture2D.MipLevels = numMips;
		}
	}

	if (numMips > 1)
	{
		auto mipLevel = 0ui8;
		m_SRVs.resize(numMips);

		for (auto &descriptor : m_SRVs)
		{
			// Setup the description of the shader resource view.
			descriptor = createSRV(format);

			if (arraySize > 1)
			{
				descriptor->Srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
				descriptor->Srv.Texture2DArray.ArraySize = arraySize;
				descriptor->Srv.Texture2DArray.MostDetailedMip = mipLevel++;
				descriptor->Srv.Texture2DArray.MipLevels = 1;
			}
			else
			{
				descriptor->Srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
				descriptor->Srv.Texture2D.MostDetailedMip = mipLevel++;
				descriptor->Srv.Texture2D.MipLevels = 1;
			}
		}
	}
}

void Texture2D::CreateUAV(uint32_t arraySize, Format format, uint8_t numMips)
{
	auto mipLevel = 0ui8;
	m_UAVs.resize(numMips);
	
	for (auto &descriptor : m_UAVs)
	{
		// Setup the description of the unordered access view.
		descriptor = createUAV(format);

		if (arraySize > 1)
		{
			descriptor->Uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
			descriptor->Uav.Texture2DArray.ArraySize = arraySize;
			descriptor->Uav.Texture2DArray.MipSlice = mipLevel++;
		}
		else
		{
			descriptor->Uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			descriptor->Uav.Texture2D.MipSlice = mipLevel++;
		}
	}
}

void Texture2D::CreateSubSRVs()
{
	const auto desc = m_resource->GetDesc();

	auto mipLevel = 1ui8;
	m_subSRVs.resize(max(desc.MipLevels, 1) - 1);

	for (auto &descriptor : m_subSRVs)
	{
		// Setup the description of the shader resource view.
		descriptor = createSRV(m_SRV->Srv.Format);

		if (desc.DepthOrArraySize > 1)
		{
			descriptor->Srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
			descriptor->Srv.Texture2DArray.ArraySize = desc.DepthOrArraySize;
			descriptor->Srv.Texture2DArray.MostDetailedMip = mipLevel;
			descriptor->Srv.Texture2DArray.MipLevels = desc.MipLevels - mipLevel;
		}
		else
		{
			descriptor->Srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			descriptor->Srv.Texture2D.MostDetailedMip = mipLevel;
			descriptor->Srv.Texture2D.MipLevels = desc.MipLevels - mipLevel;
		}
		++mipLevel;
	}
}

const DescriptorView& Texture2D::GetUAV(uint8_t i) const
{
	assert(m_UAVs.size() > i);

	return m_UAVs[i];
}

const DescriptorView& Texture2D::GetSRVLevel(uint8_t i) const
{
	assert(m_SRVs.size() > i);

	return m_SRVs[i];
}

const DescriptorView& Texture2D::GetSubSRV(uint8_t i) const
{
	assert(m_subSRVs.size() > i);

	return m_subSRVs[i];
}

//--------------------------------------------------------------------------------------
// Render target
//--------------------------------------------------------------------------------------

RenderTarget::RenderTarget(const Device& device) :
	Texture2D(device),
	m_RTVs(0)
{
}

RenderTarget::~RenderTarget()
{
}

void RenderTarget::Create(uint32_t width, uint32_t height, Format format, uint32_t arraySize,
	ResourceFlags resourceFlags, uint8_t numMips, uint8_t sampleCount, ResourceState state)
{
	create(width, height, arraySize, format, numMips, sampleCount, resourceFlags, state);

	m_RTVs.resize(arraySize);
	for (auto i = 0u; i < arraySize; ++i)
	{
		auto mipLevel = 0ui8;
		m_RTVs[i].resize(max(mipLevel, 1));
		for (auto &descriptor : m_RTVs[i])
		{
			// Setup the description of the render target view.
			descriptor = make_shared<Descriptor>();
			descriptor->ResourceType = Descriptor::RTV;
			descriptor->pResource = m_resource;
			descriptor->Rtv.Format = format;

			if (arraySize > 1)
			{
				if (sampleCount > 1)
				{
					descriptor->Rtv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
					descriptor->Rtv.Texture2DMSArray.FirstArraySlice = i;
					descriptor->Rtv.Texture2DMSArray.ArraySize = 1;
				}
				else
				{
					descriptor->Rtv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
					descriptor->Rtv.Texture2DArray.FirstArraySlice = i;
					descriptor->Rtv.Texture2DArray.ArraySize = 1;
					descriptor->Rtv.Texture2DArray.MipSlice = mipLevel++;
				}
			}
			else
			{
				if (sampleCount > 1)
					descriptor->Rtv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
				else
				{
					descriptor->Rtv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
					descriptor->Rtv.Texture2D.MipSlice = mipLevel++;
				}
			}
		}
	}
}

void RenderTarget::CreateArray(uint32_t width, uint32_t height, uint32_t arraySize, Format format,
	ResourceFlags resourceFlags, uint8_t numMips, uint8_t sampleCount, ResourceState state)
{
	create(width, height, arraySize, format, numMips, sampleCount, resourceFlags, state);

	m_RTVs.resize(1);
	m_RTVs[0].resize(max(numMips, 1));

	auto mipLevel = 0ui8;
	for (auto &descriptor : m_RTVs[0])
	{
		descriptor = make_shared<Descriptor>();
		descriptor->ResourceType = Descriptor::RTV;
		descriptor->pResource = m_resource;
		descriptor->Rtv.Format = format;

		// Setup the description of the render target view.
		if (sampleCount > 1)
		{
			descriptor->Rtv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
			descriptor->Rtv.Texture2DMSArray.ArraySize = arraySize;
		}
		else
		{
			descriptor->Rtv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
			descriptor->Rtv.Texture2DArray.ArraySize = arraySize;
			descriptor->Rtv.Texture2DArray.MipSlice = mipLevel++;
		}
	}
}

const DescriptorView& RenderTarget::GetRTV(uint32_t slice, uint8_t mipLevel) const
{
	assert(m_RTVs.size() > slice);
	assert(m_RTVs[slice].size() > mipLevel);

	return m_RTVs[slice][mipLevel];
}

uint32_t RenderTarget::GetArraySize() const
{
	return static_cast<uint32_t>(m_RTVs.size());
}

uint8_t RenderTarget::GetNumMips(uint32_t slice) const
{
	assert(m_RTVs.size() > slice);

	return static_cast<uint8_t>(m_RTVs[slice].size());
}

void RenderTarget::create(uint32_t width, uint32_t height, uint32_t arraySize, Format format,
	uint8_t numMips, uint8_t sampleCount, ResourceFlags resourceFlags, ResourceState state)
{
	const auto isPacked = static_cast<bool>(resourceFlags & BIND_PACKED_UAV);
	resourceFlags &= REMOVE_PACKED_UAV;

	const auto hasSRV = !(resourceFlags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE);
	const auto hasUAV = resourceFlags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	// Map formats
	auto formatReousrce = format;
	const auto formatUAV = isPacked ? MapToPackedFormat(formatReousrce) : format;

	// Setup the texture description.
	const auto desc = CD3DX12_RESOURCE_DESC::Tex2D(formatReousrce, width, height, arraySize,
		numMips, sampleCount, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | resourceFlags);

	// Determine initial state
	m_state = state ? state : D3D12_RESOURCE_STATE_RENDER_TARGET;

	// Create the render target texture.
	ThrowIfFailed(m_device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE, &desc, m_state, nullptr, IID_PPV_ARGS(&m_resource)));

	// Create SRV
	if (hasSRV)
		CreateSRV(arraySize, format, numMips, sampleCount);

	// Create UAV
	if (hasUAV)
		CreateUAV(arraySize, formatUAV, numMips);
}

//--------------------------------------------------------------------------------------
// Depth stencil
//--------------------------------------------------------------------------------------

DepthStencil::DepthStencil(const Device& device) :
	Texture2D(device),
	m_DSVs(0),
	m_DSVROs(0),
	m_SRVStencil(0),
	m_dsvHandles(0),
	m_dsvROHandles(0),
	m_pDescriptorTablePool(nullptr)
{
}

DepthStencil::~DepthStencil()
{
}

void DepthStencil::SetDescriptorTablePool(DescriptorTablePool& descriptorTablePool)
{
	m_pDescriptorTablePool = &descriptorTablePool;
}

void DepthStencil::Create(uint32_t width, uint32_t height, Format format, ResourceFlags resourceFlags,
	uint32_t arraySize, uint8_t numMips, uint8_t sampleCount, ResourceState state,
	uint8_t clearStencil, float clearDepth)
{
	const auto hasSRV = !(resourceFlags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE);

	// Map formats
	auto formatReousrce = format;
	auto formatDepth = DXGI_FORMAT_UNKNOWN;
	auto formatStencil = DXGI_FORMAT_UNKNOWN;

	if (hasSRV)
	{
		switch (format)
		{
		case DXGI_FORMAT_D24_UNORM_S8_UINT:
			formatReousrce = DXGI_FORMAT_R24G8_TYPELESS;
			formatDepth = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
			formatStencil = DXGI_FORMAT_X24_TYPELESS_G8_UINT;
			break;
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
			formatReousrce = DXGI_FORMAT_R32G8X24_TYPELESS;
			formatDepth = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
			formatStencil = DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;
			break;
		case DXGI_FORMAT_D16_UNORM:
			formatReousrce = DXGI_FORMAT_R16_TYPELESS;
			formatDepth = DXGI_FORMAT_R16_UNORM;
			break;
		default:
			formatReousrce = DXGI_FORMAT_R32_TYPELESS;
			formatDepth = DXGI_FORMAT_R32_FLOAT;
		}
	}

	// Setup the render depth stencil description.
	{
		const auto desc = CD3DX12_RESOURCE_DESC::Tex2D(formatReousrce, width, height, arraySize,
			numMips, sampleCount, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | resourceFlags);

		// Determine initial state
		m_state = state ? state : D3D12_RESOURCE_STATE_DEPTH_WRITE;

		// Optimized clear value
		D3D12_CLEAR_VALUE clearValue;
		clearValue.Format = format;
		clearValue.DepthStencil.Depth = clearDepth;
		clearValue.DepthStencil.Stencil = clearStencil;

		// Create the depth stencil texture.
		ThrowIfFailed(m_device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE, &desc, m_state, &clearValue, IID_PPV_ARGS(&m_resource)));
	}

	if (hasSRV)
	{
		// Create SRV
		if (hasSRV)
			CreateSRV(arraySize, formatDepth, numMips, sampleCount);

		// Has stencil
		if (formatStencil)
		{
			auto& descriptor = m_SRVStencil;
			descriptor = createSRV(format);

			if (arraySize > 1)
			{
				if (sampleCount > 1)
				{
					descriptor->Srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
					descriptor->Srv.Texture2DMSArray.ArraySize = arraySize;
				}
				else
				{
					descriptor->Srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
					descriptor->Srv.Texture2DArray.ArraySize = arraySize;
					descriptor->Srv.Texture2DArray.MipLevels = numMips;
				}
			}
			else
			{
				if (sampleCount > 1)
					descriptor->Srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
				else
				{
					descriptor->Srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
					descriptor->Srv.Texture2D.MipLevels = numMips;
				}
			}
		}
	}

	numMips = max(numMips, 1);
	m_DSVs.resize(numMips);
	m_DSVROs.resize(numMips);

	if (m_pDescriptorTablePool)
	{
		m_dsvHandles.resize(numMips);
		m_dsvROHandles.resize(numMips);
	}

	for (auto i = 0ui8; i < numMips; ++i)
	{
		// Setup the description of the depth stencil view.
		auto& descriptor = m_DSVs[i];
		descriptor = make_shared<Descriptor>();
		descriptor->ResourceType = Descriptor::DSV;
		descriptor->pResource = m_resource;
		descriptor->Dsv.Format = format;

		if (arraySize > 1)
		{
			if (sampleCount > 1)
			{
				descriptor->Dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
				descriptor->Dsv.Texture2DMSArray.ArraySize = arraySize;
			}
			else
			{
				descriptor->Dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
				descriptor->Dsv.Texture2DArray.ArraySize = arraySize;
				descriptor->Dsv.Texture2DArray.MipSlice = i;
			}
		}
		else
		{
			if (sampleCount > 1)
				descriptor->Dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
			else
			{
				descriptor->Dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
				descriptor->Dsv.Texture2D.MipSlice = i;
			}
		}

		if (m_pDescriptorTablePool)
		{
			Util::DescriptorTable dsvTable;
			dsvTable.SetDepthStencil(descriptor);
			m_dsvHandles[i] = dsvTable.GetDepthStencilHandle(*m_pDescriptorTablePool);
		}

		// Read-only depth stencil
		if (hasSRV)
		{
			// Setup the description of the depth stencil view.
			auto& descriptor = m_DSVROs[i];
			descriptor = make_shared<Descriptor>();
			*descriptor = *m_DSVs[i];

			descriptor->Dsv.Flags = formatStencil ? D3D12_DSV_FLAG_READ_ONLY_DEPTH |
				D3D12_DSV_FLAG_READ_ONLY_STENCIL : D3D12_DSV_FLAG_READ_ONLY_DEPTH;

			if (m_pDescriptorTablePool)
			{
				Util::DescriptorTable dsvTable;
				dsvTable.SetDepthStencil(descriptor);
				m_dsvROHandles[i] = dsvTable.GetDepthStencilHandle(*m_pDescriptorTablePool);
			}
		}
		else m_DSVROs[i] = m_DSVs[i];
	}
}

const DescriptorView& DepthStencil::GetDSV(uint8_t mipLevel) const
{
	assert(m_DSVs.size() > mipLevel);

	return m_DSVs[mipLevel];
}

const DescriptorView& DepthStencil::GetDSVReadOnly(uint8_t mipLevel) const
{
	assert(m_DSVROs.size() > mipLevel);

	return m_DSVROs[mipLevel];
}

const DescriptorView& DepthStencil::GetSRVStencil() const
{
	return m_SRVStencil;
}

const DepthStencilHandle& DepthStencil::GetDsvHandle(uint8_t mipLevel) const
{
	assert(m_dsvHandles.size() > mipLevel);

	return m_dsvHandles[mipLevel];
}

const DepthStencilHandle& DepthStencil::GetDsvHandleReadOnly(uint8_t mipLevel) const
{
	assert(m_dsvROHandles.size() > mipLevel);

	return m_dsvROHandles[mipLevel];
}

const uint8_t DepthStencil::GetNumMips() const
{
	return static_cast<uint8_t>(m_DSVs.size());
}

//--------------------------------------------------------------------------------------
// Buffer base
//--------------------------------------------------------------------------------------

BufferBase::BufferBase(const Device& device) :
	ResourceBase(device)
{
}

BufferBase::~BufferBase()
{
}

void BufferBase::Upload(const GraphicsCommandList& commandList, Resource& resourceUpload,
	const void *pData, ResourceState dstState)
{
	const auto desc = m_resource->GetDesc();
	const auto uploadBufferSize = GetRequiredIntermediateSize(m_resource.Get(), 0, 1);

	// Create the GPU upload buffer.
	ThrowIfFailed(m_device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&resourceUpload)));

	// Copy data to the intermediate upload heap and then schedule a copy 
	// from the upload heap to the buffer.
	D3D12_SUBRESOURCE_DATA subresourceData;
	subresourceData.pData = pData;
	subresourceData.RowPitch = static_cast<uint32_t>(uploadBufferSize);
	subresourceData.SlicePitch = subresourceData.RowPitch;

	dstState = dstState ? dstState : m_state;
	if (m_state != D3D12_RESOURCE_STATE_COPY_DEST)
		commandList->ResourceBarrier(1, &Transition(D3D12_RESOURCE_STATE_COPY_DEST));
	UpdateSubresources(commandList.Get(), m_resource.Get(), resourceUpload.Get(), 0, 0, 1, &subresourceData);
	commandList->ResourceBarrier(1, &Transition(dstState));
}

//--------------------------------------------------------------------------------------
// Raw buffer
//--------------------------------------------------------------------------------------

RawBuffer::RawBuffer(const Device& device) :
	BufferBase(device),
	m_UAV(nullptr)
{
}

RawBuffer::~RawBuffer()
{
}

void RawBuffer::Create(uint32_t byteWidth, ResourceFlags resourceFlags, PoolType poolType, ResourceState state)
{
	const auto hasSRV = !(resourceFlags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE);
	const auto hasUAV = resourceFlags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

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

	ThrowIfFailed(m_device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(poolType),
		D3D12_HEAP_FLAG_NONE, &desc, m_state, nullptr, IID_PPV_ARGS(&m_resource)));

	// Create SRV
	if (hasSRV)
		CreateSRV(byteWidth);

	// Create UAV
	if (hasUAV)
		CreateUAV(byteWidth);
}

void RawBuffer::CreateSRV(uint32_t byteWidth)
{
	auto& descriptor = m_SRV;
	descriptor = createSRV(DXGI_FORMAT_R32_TYPELESS);

	const auto stride = 4u;
	descriptor->Srv.Buffer.FirstElement = 0;
	descriptor->Srv.Buffer.NumElements = byteWidth / stride;
	descriptor->Srv.Buffer.StructureByteStride = stride;
	descriptor->Srv.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
}

void RawBuffer::CreateUAV(uint32_t byteWidth)
{
	auto& descriptor = m_UAV;
	descriptor = createUAV(DXGI_FORMAT_R32_TYPELESS);

	const auto stride = 4u;
	descriptor->Uav.Buffer.FirstElement = 0;
	descriptor->Uav.Buffer.NumElements = byteWidth / stride;
	descriptor->Uav.Buffer.StructureByteStride = stride;
	descriptor->Uav.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
}

const DescriptorView& RawBuffer::GetUAV() const
{
	return m_UAV;
}

//--------------------------------------------------------------------------------------
// Vertex buffer
//--------------------------------------------------------------------------------------

VertexBuffer::VertexBuffer(const Device& device) :
	RawBuffer(device),
	m_VBV()
{
}

VertexBuffer::~VertexBuffer()
{
}

void VertexBuffer::Create(uint32_t byteWidth, uint32_t stride, ResourceFlags resourceFlags,
	PoolType poolType, ResourceState state)
{
	const auto hasSRV = !(resourceFlags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE);
	const auto hasUAV = resourceFlags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	// Determine initial state
	if (state == 0)
	{
		state = hasSRV ? D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE |
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE :
			D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		state = hasUAV ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS : state;
	}

	RawBuffer::Create(byteWidth, resourceFlags, poolType, state);

	// Create vertex buffer view
	m_VBV.BufferLocation = m_resource->GetGPUVirtualAddress();
	m_VBV.StrideInBytes = stride;
	m_VBV.SizeInBytes = byteWidth;
}

const VertexBufferView& VertexBuffer::GetVBV() const
{
	return m_VBV;
}

//--------------------------------------------------------------------------------------
// Index buffer
//--------------------------------------------------------------------------------------

IndexBuffer::IndexBuffer(const Device& device) :
	BufferBase(device),
	m_IBV()
{
}

IndexBuffer::~IndexBuffer()
{
}

void IndexBuffer::Create(uint32_t byteWidth, Format format, ResourceFlags resourceFlags,
	PoolType poolType, ResourceState state)
{
	const auto hasSRV = !(resourceFlags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE);

	// Setup the buffer description.
	const auto desc = CD3DX12_RESOURCE_DESC::Buffer(byteWidth, D3D12_RESOURCE_FLAGS(resourceFlags));

	// Determine initial state
	if (state) m_state = state;
	else m_state = hasSRV ? D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE |
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_INDEX_BUFFER;
	
	ThrowIfFailed(m_device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(poolType),
		D3D12_HEAP_FLAG_NONE, &desc, m_state, nullptr, IID_PPV_ARGS(&m_resource)));

	// Create SRV
	//if (hasSRV)
		//CreateSRV(byteWidth);

	// Create index buffer view
	m_IBV.BufferLocation = m_resource->GetGPUVirtualAddress();
	m_IBV.SizeInBytes = byteWidth;
	m_IBV.Format = format;
}

const IndexBufferView& IndexBuffer::GetIBV() const
{
	return m_IBV;
}
