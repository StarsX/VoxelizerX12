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

		const Resource		&GetResource() const;
		const Descriptor	&GetCBV() const;

	protected:
		void allocateDescriptorPool(uint32_t numDescriptors);

		void			*m_pDataBegin;

		Resource		m_resource;
		Descriptor		m_CBV;

		DescriptorPool	m_cbvPool;
		
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

		void Barrier(const GraphicsCommandList &commandList, ResourceState dstState);

		const Resource		&GetResource() const;
		const Descriptor	&GetSRV() const;

		ResourceBarrier		Transition(ResourceState dstState);

		//static void CreateReadBuffer(const Device &device,
			//CPDXBuffer &pDstBuffer, const CPDXBuffer &pSrcBuffer);
	protected:
		void allocateDescriptorPool(uint32_t numDescriptors);

		Resource		m_resource;
		Descriptor		m_SRV;
		Descriptor		m_srvUavCurrent;

		ResourceState	m_state;

		DescriptorPool	m_srvUavPool;
		uint32_t		m_strideSrvUav;

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
		void Upload(const GraphicsCommandList &commandList, Resource &resourceUpload, const void *pData,
			uint8_t stride = sizeof(float), ResourceState dstState = ResourceState(0));
		void CreateSRV(uint32_t arraySize, Format format = Format(0),
			uint8_t numMips = 1, uint8_t sampleCount = 1);
		void CreateUAV(uint32_t arraySize, Format format = Format(0), uint8_t numMips = 1);
		void CreateSubSRVs(Format format = Format(0));

		const Descriptor	&GetUAV(uint8_t i = 0) const;
		const Descriptor	&GetSRVLevel(uint8_t i) const;
		const Descriptor	&GetSubSRV(uint8_t i) const;

	protected:
		Resource m_counter;
		std::vector<Descriptor>	m_UAVs;
		std::vector<Descriptor>	m_SRVs;
		std::vector<Descriptor>	m_subSRVs;
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

		const Descriptor	&GetRTV(uint32_t slice = 0, uint8_t mipLevel = 0) const;
		uint32_t			GetArraySize() const;
		uint8_t				GetNumMips(uint32_t slice = 0) const;

	protected:
		void create(uint32_t width, uint32_t height, uint32_t arraySize, Format format,
			uint8_t numMips, uint8_t sampleCount, ResourceFlags resourceFlags, ResourceState state);
		void allocateRtvPool(uint32_t numDescriptors);

		std::vector<std::vector<Descriptor>> m_RTVs;
		Descriptor	m_rtvCurrent;

		DescriptorPool	m_rtvPool;
		uint32_t		m_strideRtv;
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

		void Create(uint32_t width, uint32_t height, Format format = DXGI_FORMAT_D24_UNORM_S8_UINT,
			ResourceFlags resourceFlags = ResourceFlags(0), uint32_t arraySize = 1, uint8_t numMips = 1,
			uint8_t sampleCount = 1, ResourceState state = ResourceState(0),
			uint8_t clearStencil = 0, float clearDepth = 1.0f);

		const Descriptor	&GetDSV(uint8_t mipLevel = 0) const;
		const Descriptor	&GetDSVReadOnly(uint8_t mipLevel = 0) const;
		const Descriptor	&GetSRVStencil() const;

		const uint8_t		GetNumMips() const;

	protected:
		void allocateDsvPool(uint32_t numDescriptors);

		std::vector<Descriptor> m_DSVs;
		std::vector<Descriptor> m_DSVROs;
		Descriptor	m_SRVStencil;
		Descriptor	m_dsvCurrent;

		DescriptorPool	m_dsvPool;
		uint32_t		m_strideDsv;
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
		void CreateSubSRVs(Format format = Format(0));

		const Descriptor	&GetUAV(uint8_t i = 0) const;
		const Descriptor	&GetSRVLevel(uint8_t i) const;
		const Descriptor	&GetSubSRV(uint8_t i) const;

	protected:
		Resource m_counter;
		std::vector<Descriptor>	m_UAVs;
		std::vector<Descriptor>	m_SRVs;
		std::vector<Descriptor>	m_subSRVs;
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

		void Upload(const GraphicsCommandList &commandList, Resource &resourceUpload,
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

		const Descriptor &GetUAV() const;

	protected:
		void create(uint32_t byteWidth, ResourceFlags resourceFlags, PoolType poolType,
			ResourceState state, bool hasSRV, bool hasUAV);

		Resource m_counter;
		Descriptor m_UAV;
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
		void CreateSRV(uint32_t numElements, Format format);
		void CreateUAV(uint32_t numElements, Format format);
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
