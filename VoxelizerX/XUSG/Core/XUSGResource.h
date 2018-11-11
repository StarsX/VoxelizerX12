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
		ConstantBuffer();
		virtual ~ConstantBuffer();

		void Create(const Device &device, uint32_t byteWidth, uint32_t cbvSize);

		void *Map();
		void Unmap();

		const Resource		&GetResource() const;
		const Descriptor	&GetCBV() const;

	protected:
		void allocateDescriptorPool(uint32_t numDescriptors);

		Device			m_device;

		Resource		m_resource;
		DescriptorPool	m_cbvPool;
		Descriptor		m_CBV;

		void			*m_pDataBegin;
	};

	//--------------------------------------------------------------------------------------
	// Resource base
	//--------------------------------------------------------------------------------------
	class ResourceBase
	{
	public:
		ResourceBase();
		virtual ~ResourceBase();

		void Barrier(const GraphicsCommandList &commandList, ResourceState dstState);

		const Resource		&GetResource() const;
		const Descriptor	&GetSRV() const;

		ResourceBarrier		Transition(ResourceState dstState);

		//static void CreateReadBuffer(const Device &device,
			//CPDXBuffer &pDstBuffer, const CPDXBuffer &pSrcBuffer);
	protected:
		void setDevice(const Device &device);
		void allocateDescriptorPool(uint32_t numDescriptors);

		Device			m_device;

		Resource		m_resource;
		DescriptorPool	m_srvUavPool;
		Descriptor		m_SRV;
		Descriptor		m_srvUavCurrent;

		ResourceState	m_state;
		uint32_t		m_strideSrvUav;
	};

	//--------------------------------------------------------------------------------------
	// 2D Texture
	//--------------------------------------------------------------------------------------
	class Texture2D :
		public ResourceBase
	{
	public:
		Texture2D();
		virtual ~Texture2D();

		void Create(const Device &device, uint32_t width, uint32_t height, Format format,
			uint32_t arraySize = 1, ResourceFlags resourceFlags = ResourceFlags(0),
			uint8_t numMips = 1, uint8_t sampleCount = 1, PoolType poolType = PoolType(1),
			ResourceState state = ResourceState(0));
		void Upload(const GraphicsCommandList &commandList, Resource &resourceUpload, const void *pData,
			uint8_t stride = sizeof(float), ResourceState dstState = ResourceState(0));
		void CreateSRV(uint32_t arraySize, Format format = Format(0),
			uint8_t numMips = 1, uint8_t sampleCount = 1);
		void CreateSRVs(uint32_t arraySize, uint8_t numMips, Format format = Format(0), uint8_t sampleCount = 1);
		void CreateUAVs(uint32_t arraySize, Format format = Format(0), uint8_t numMips = 1);
		void CreateSubSRVs(Format format = Format(0));

		Descriptor GetUAV(uint8_t i = 0) const;
		Descriptor GetSRVAtLevel(uint8_t i) const;
		Descriptor GetSubSRV(uint8_t i) const;

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
		RenderTarget();
		virtual ~RenderTarget();

		void Create(const Device &device, uint32_t width, uint32_t height, Format format,
			uint32_t arraySize = 1, ResourceFlags resourceFlags = ResourceFlags(0),
			uint8_t numMips = 1, uint8_t sampleCount = 1, ResourceState state = ResourceState(0));
		void CreateArray(const Device &device, uint32_t width, uint32_t height, uint32_t arraySize,
			Format format, ResourceFlags resourceFlags = ResourceFlags(0), uint8_t numMips = 1,
			uint8_t sampleCount = 1, ResourceState state = ResourceState(0));
		//void Populate(const CPDXShaderResourceView &pSRVSrc, const spShader &pShader,
			//const uint8_t uSRVSlot = 0, const uint8_t uSlice = 0, const uint8_t uMip = 0);

		Descriptor	GetRTV(uint32_t slice = 0, uint8_t mipLevel = 0) const;
		uint32_t	GetArraySize() const;
		uint8_t		GetNumMips(uint32_t slice = 0) const;

	protected:
		void create(const Device &device, uint32_t width, uint32_t height,
			uint32_t arraySize, Format format, uint8_t numMips, uint8_t sampleCount,
			ResourceFlags resourceFlags, ResourceState state);
		void allocateRtvPool(uint32_t numDescriptors);

		DescriptorPool	m_rtvPool;
		std::vector<std::vector<Descriptor>> m_RTVs;
		Descriptor		m_rtvCurrent;

		uint32_t		m_strideRtv;
	};

	//--------------------------------------------------------------------------------------
	// Depth stencil
	//--------------------------------------------------------------------------------------
	class DepthStencil :
		public Texture2D
	{
	public:
		DepthStencil();
		virtual ~DepthStencil();

		void Create(const Device &device, uint32_t width, uint32_t height, Format format =
			DXGI_FORMAT_D24_UNORM_S8_UINT, ResourceFlags resourceFlags = ResourceFlags(0),
			uint32_t arraySize = 1, uint8_t numMips = 1, uint8_t sampleCount = 1,
			ResourceState state = ResourceState(0), uint8_t clearStencil = 0,
			float clearDepth = 1.0f);

		Descriptor GetDSV(uint8_t mipLevel = 0) const;
		Descriptor GetDSVReadOnly(uint8_t mipLevel = 0) const;
		const Descriptor &GetSRVStencil() const;

		const uint8_t		GetNumMips() const;

	protected:
		void allocateDsvPool(uint32_t numDescriptors);

		DescriptorPool m_dsvPool;
		std::vector<Descriptor> m_DSVs;
		std::vector<Descriptor> m_DSVROs;
		Descriptor	m_SRVStencil;
		Descriptor	m_dsvCurrent;

		uint32_t		m_strideDsv;
	};

	//--------------------------------------------------------------------------------------
	// 3D Texture
	//--------------------------------------------------------------------------------------
	class Texture3D :
		public ResourceBase
	{
	public:
		Texture3D();
		virtual ~Texture3D();

		void Create(const Device &device, uint32_t width, uint32_t height, uint32_t depth,
			Format format, ResourceFlags resourceFlags = ResourceFlags(0), uint8_t numMips = 1,
			PoolType poolType = PoolType(1), ResourceState state = ResourceState(0));
		void CreateSRV(Format format = Format(0), uint8_t numMips = 1);
		void CreateSRVs(uint8_t numMips, Format format = Format(0));
		void CreateUAVs(Format format = Format(0), uint8_t numMips = 1);
		void CreateSubSRVs(Format format = Format(0));

		Descriptor GetUAV(uint8_t i = 0) const;
		Descriptor GetSRVLevel(uint8_t i) const;
		Descriptor GetSubSRV(uint8_t i) const;

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
		BufferBase();
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
		RawBuffer();
		virtual ~RawBuffer();

		void Create(const Device &device, uint32_t byteWidth, ResourceFlags resourceFlags = ResourceFlags(0),
			PoolType poolType = PoolType(1), ResourceState state = ResourceState(0));
		void CreateSRV(uint32_t byteWidth);
		void CreateUAV(uint32_t byteWidth);

		const Descriptor &GetUAV() const;

	protected:
		void create(const Device &device, uint32_t byteWidth, ResourceFlags resourceFlags,
			PoolType poolType, ResourceState state, bool hasSRV, bool hasUAV);

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
		VertexBuffer();
		virtual ~VertexBuffer();

		void Create(const Device &device, uint32_t byteWidth, uint32_t stride,
			ResourceFlags resourceFlags = ResourceFlags(0), PoolType poolType = PoolType(1),
			ResourceState state = ResourceState(0));

		const VertexBufferView &GetVBV() const;

	protected:
		VertexBufferView m_VBV;
	};

	//--------------------------------------------------------------------------------------
	// Index buffer
	//--------------------------------------------------------------------------------------
	class IndexBuffer :
		public RawBuffer
	{
	public:
		IndexBuffer();
		virtual ~IndexBuffer();

		void Create(const Device &device, uint32_t byteWidth, Format format = Format(42),
			ResourceFlags resourceFlags = ResourceFlags(0x8), PoolType poolType = PoolType(1),
			ResourceState state = ResourceState(0));
		
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
		TypedBuffer();
		virtual ~TypedBuffer();

		void Create(const Device &device, uint32_t numElements, uint32_t stride, Format format,
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
		StructuredBuffer();
		virtual ~StructuredBuffer();

		void Create(const Device &device, uint32_t numElements, uint32_t stride,
			ResourceFlags resourceFlags = ResourceFlags(0), PoolType poolType = PoolType(1),
			ResourceState state = ResourceState(0));
		void CreateSRV(uint32_t numElements, uint32_t stride);
		void CreateUAV(uint32_t numElements, uint32_t stride);
	};
}
