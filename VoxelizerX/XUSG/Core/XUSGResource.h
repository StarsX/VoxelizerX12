//--------------------------------------------------------------------------------------
// By Stars XU Tianchen
//--------------------------------------------------------------------------------------

#pragma once

#include "XUSGDescriptor.h"

#define	BIND_PACKED_UAV	ResourceFlags(0x4 | 0x8000)

namespace XUSG
{
	//--------------------------------------------------------------------------------------
	// Constant buffer
	//--------------------------------------------------------------------------------------
	class ConstantBuffer
	{
	public:
		ConstantBuffer(const Device &device);
		virtual ~ConstantBuffer();

		void Create(uint32_t byteWidth, uint32_t cbvSize);

		void *Map();
		void Unmap();

		const Resource			&GetResource() const;
		const DescriptorView	&GetCBV() const;

	protected:
		Resource		m_resource;
		DescriptorView	m_CBV;
		void			*m_pDataBegin;

		Device			m_device;
	};

	//--------------------------------------------------------------------------------------
	// Resource base
	//--------------------------------------------------------------------------------------
	class ResourceBase
	{
	public:
		ResourceBase(const Device &device);
		virtual ~ResourceBase();

		const Resource			&GetResource() const;
		const DescriptorView	&GetSRV() const;

		ResourceBarrier Transition(ResourceState dstState);

		//static void CreateReadBuffer(const Device &device,
			//CPDXBuffer &pDstBuffer, const CPDXBuffer &pSrcBuffer);
	protected:
		DescriptorView createSRV(Format format);
		DescriptorView createUAV(Format format);

		Resource		m_resource;
		DescriptorView	m_SRV;

		ResourceState	m_state;

		Device			m_device;
	};

	//--------------------------------------------------------------------------------------
	// 2D Texture
	//--------------------------------------------------------------------------------------
	class Texture2D :
		public ResourceBase
	{
	public:
		Texture2D(const Device &device);
		virtual ~Texture2D();

		void Create(uint32_t width, uint32_t height, Format format, uint32_t arraySize = 1,
			ResourceFlags resourceFlags = ResourceFlags(0), uint8_t numMips = 1, uint8_t sampleCount = 1,
			PoolType poolType = PoolType(1), ResourceState state = ResourceState(0));
		void Upload(const GraphicsCommandList& commandList, Resource& resourceUpload, const void *pData,
			uint8_t stride = sizeof(float), ResourceState dstState = ResourceState(0));
		void CreateSRV(uint32_t arraySize, Format format = Format(0),
			uint8_t numMips = 1, uint8_t sampleCount = 1);
		void CreateUAV(uint32_t arraySize, Format format = Format(0), uint8_t numMips = 1);
		void CreateSubSRVs();

		const DescriptorView	&GetUAV(uint8_t i = 0) const;
		const DescriptorView	&GetSRVLevel(uint8_t i) const;
		const DescriptorView	&GetSubSRV(uint8_t i) const;

	protected:
		std::vector<DescriptorView>	m_UAVs;
		std::vector<DescriptorView>	m_SRVs;
		std::vector<DescriptorView>	m_subSRVs;
	};

	//--------------------------------------------------------------------------------------
	// Render target
	//--------------------------------------------------------------------------------------
	class RenderTarget :
		public Texture2D
	{
	public:
		RenderTarget(const Device &device);
		virtual ~RenderTarget();

		void Create(uint32_t width, uint32_t height, Format format, uint32_t arraySize = 1,
			ResourceFlags resourceFlags = ResourceFlags(0), uint8_t numMips = 1,
			uint8_t sampleCount = 1, ResourceState state = ResourceState(0));
		void CreateArray(uint32_t width, uint32_t height, uint32_t arraySize, Format format,
			ResourceFlags resourceFlags = ResourceFlags(0), uint8_t numMips = 1,
			uint8_t sampleCount = 1, ResourceState state = ResourceState(0));
		//void Populate(const CPDXShaderResourceView &pSRVSrc, const spShader &pShader,
			//const uint8_t uSRVSlot = 0, const uint8_t uSlice = 0, const uint8_t uMip = 0);

		const DescriptorView	&GetRTV(uint32_t slice = 0, uint8_t mipLevel = 0) const;
		uint32_t				GetArraySize() const;
		uint8_t					GetNumMips(uint32_t slice = 0) const;

	protected:
		void create(uint32_t width, uint32_t height, uint32_t arraySize, Format format,
			uint8_t numMips, uint8_t sampleCount, ResourceFlags resourceFlags, ResourceState state);

		std::vector<std::vector<DescriptorView>> m_RTVs;
	};

	//--------------------------------------------------------------------------------------
	// Depth stencil
	//--------------------------------------------------------------------------------------
	class DepthStencil :
		public Texture2D
	{
	public:
		DepthStencil(const Device &device);
		virtual ~DepthStencil();

		void SetDescriptorTablePool(DescriptorTablePool& descriptorTablePool);
		void Create(uint32_t width, uint32_t height, Format format = DXGI_FORMAT_D24_UNORM_S8_UINT,
			ResourceFlags resourceFlags = ResourceFlags(0), uint32_t arraySize = 1, uint8_t numMips = 1,
			uint8_t sampleCount = 1, ResourceState state = ResourceState(0),
			uint8_t clearStencil = 0, float clearDepth = 1.0f);

		const DescriptorView		&GetDSV(uint8_t mipLevel = 0) const;
		const DescriptorView		&GetDSVReadOnly(uint8_t mipLevel = 0) const;
		const DescriptorView		&GetSRVStencil() const;

		const DepthStencilHandle	&GetDsvHandle(uint8_t mipLevel = 0) const;
		const DepthStencilHandle	&GetDsvHandleReadOnly(uint8_t mipLevel = 0) const;

		const uint8_t				GetNumMips() const;

	protected:
		std::vector<DescriptorView> m_DSVs;
		std::vector<DescriptorView> m_DSVROs;
		DescriptorView m_SRVStencil;

		std::vector<DepthStencilHandle> m_dsvHandles;
		std::vector<DepthStencilHandle> m_dsvROHandles;

		DescriptorTablePool *m_pDescriptorTablePool;
	};

	//--------------------------------------------------------------------------------------
	// 3D Texture
	//--------------------------------------------------------------------------------------
	class Texture3D :
		public ResourceBase
	{
	public:
		Texture3D(const Device &device);
		virtual ~Texture3D();

		void Create(uint32_t width, uint32_t height, uint32_t depth, Format format,
			ResourceFlags resourceFlags = ResourceFlags(0), uint8_t numMips = 1,
			PoolType poolType = PoolType(1), ResourceState state = ResourceState(0));
		void CreateSRV(Format format = Format(0), uint8_t numMips = 1);
		void CreateUAV(Format format = Format(0), uint8_t numMips = 1);
		void CreateSubSRVs();

		const DescriptorView	&GetUAV(uint8_t i = 0) const;
		const DescriptorView	&GetSRVLevel(uint8_t i) const;
		const DescriptorView	&GetSubSRV(uint8_t i) const;

	protected:
		std::vector<DescriptorView>	m_UAVs;
		std::vector<DescriptorView>	m_SRVs;
		std::vector<DescriptorView>	m_subSRVs;
	};

	//--------------------------------------------------------------------------------------
	// Buffer base
	//--------------------------------------------------------------------------------------
	class BufferBase :
		public ResourceBase
	{
	public:
		BufferBase(const Device &device);
		virtual ~BufferBase();

		void Upload(const GraphicsCommandList& commandList, Resource& resourceUpload,
			const void *pData, ResourceState dstState = ResourceState(0));
	};

	//--------------------------------------------------------------------------------------
	// Raw buffer
	//--------------------------------------------------------------------------------------
	class RawBuffer :
		public BufferBase
	{
	public:
		RawBuffer(const Device &device);
		virtual ~RawBuffer();

		void Create(uint32_t byteWidth, ResourceFlags resourceFlags = ResourceFlags(0),
			PoolType poolType = PoolType(1), ResourceState state = ResourceState(0));
		void CreateSRV(uint32_t byteWidth);
		void CreateUAV(uint32_t byteWidth);

		const DescriptorView &GetUAV() const;

	protected:
		void create(uint32_t byteWidth, ResourceFlags resourceFlags, PoolType poolType,
			ResourceState state, bool hasSRV, bool hasUAV);

		DescriptorView m_UAV;
	};

	//--------------------------------------------------------------------------------------
	// Vertex buffer
	//--------------------------------------------------------------------------------------
	class VertexBuffer :
		public RawBuffer
	{
	public:
		VertexBuffer(const Device &device);
		virtual ~VertexBuffer();

		void Create(uint32_t byteWidth, uint32_t stride, ResourceFlags resourceFlags = ResourceFlags(0),
			PoolType poolType = PoolType(1), ResourceState state = ResourceState(0));

		const VertexBufferView &GetVBV() const;

	protected:
		VertexBufferView m_VBV;
	};

	//--------------------------------------------------------------------------------------
	// Index buffer
	//--------------------------------------------------------------------------------------
	class IndexBuffer :
		public BufferBase
	{
	public:
		IndexBuffer(const Device &device);
		virtual ~IndexBuffer();

		void Create(uint32_t byteWidth, Format format = Format(42), ResourceFlags resourceFlags = ResourceFlags(8),
			PoolType poolType = PoolType(1), ResourceState state = ResourceState(0));
		
		const IndexBufferView &GetIBV() const;

	protected:
		IndexBufferView m_IBV;
	};

	//--------------------------------------------------------------------------------------
	// Typed buffer
	//--------------------------------------------------------------------------------------
	class TypedBuffer :
		public RawBuffer
	{
	public:
		TypedBuffer(const Device &device);
		virtual ~TypedBuffer();

		void Create(uint32_t numElements, uint32_t stride, Format format,
			ResourceFlags resourceFlags = ResourceFlags(0), PoolType poolType = PoolType(1),
			ResourceState state = ResourceState(0));
		void CreateSRV(uint32_t numElements, uint32_t stride, Format format);
		void CreateUAV(uint32_t numElements, uint32_t stride, Format format);
	};

	//--------------------------------------------------------------------------------------
	// Structured buffer
	//--------------------------------------------------------------------------------------
	class StructuredBuffer :
		public RawBuffer
	{
	public:
		StructuredBuffer(const Device &device);
		virtual ~StructuredBuffer();

		void Create(uint32_t numElements, uint32_t stride, ResourceFlags resourceFlags = ResourceFlags(0),
			PoolType poolType = PoolType(1), ResourceState state = ResourceState(0));
		void CreateSRV(uint32_t numElements, uint32_t stride);
		void CreateUAV(uint32_t numElements, uint32_t stride);
	};
}
