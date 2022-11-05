//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "Core/XUSG.h"
#include "SharedConst.h"

class Voxelizer
{
public:
	enum Method : uint8_t
	{
		TRI_PROJ,
		TRI_PROJ_TESS,
		TRI_PROJ_UNION,

		NUM_METHOD
	};

	Voxelizer();
	virtual ~Voxelizer();

	bool Init(XUSG::CommandList* pCommandList, const XUSG::DescriptorTableLib::sptr& descriptorTableLib,
		uint32_t width, uint32_t height, XUSG::Format rtFormat, XUSG::Format dsFormat,
		std::vector<XUSG::Resource::uptr>& uploaders, const char* fileName, const DirectX::XMFLOAT4& posScale);
	void UpdateFrame(uint8_t frameIndex, DirectX::CXMVECTOR eyePt, DirectX::CXMMATRIX viewProj);
	void Render(XUSG::CommandList* pCommandList, bool solid, Method voxMethod, uint8_t frameIndex,
		const XUSG::Descriptor& rtv, const XUSG::Descriptor& dsv);

	static const uint8_t FrameCount = FRAME_COUNT;

protected:
	enum RenderPass : uint8_t
	{
		PASS_VOXELIZE,
		PASS_VOXELIZE_SOLID,
		PASS_VOXELIZE_TESS,
		PASS_VOXELIZE_TESS_SOLID,
		PASS_VOXELIZE_UNION,
		PASS_VOXELIZE_UNION_SOLID,
		PASS_FILL_SOLID,
		PASS_DRAW_AS_BOX,
		PASS_RAY_CAST,

		NUM_PASS
	};

	enum CBVTable : uint8_t
	{
		CBV_TABLE_VOXELIZE,
		CBV_TABLE_PER_MIP,
		CBV_TABLE_MATRICES,
		CBV_TABLE_PER_OBJ = CBV_TABLE_MATRICES + FrameCount,

		NUM_CBV_TABLE = CBV_TABLE_PER_OBJ + FrameCount
	};

	enum SRVTable : uint8_t
	{
		SRV_TABLE_VB_IB,
		SRV_K_DEPTH,
		SRV_TABLE_GRID,
#if	USE_MUTEX
		SRV_TABLE_GRID_XYZ,
#endif

		NUM_SRV_TABLE
	};

	enum UAVTable : uint8_t
	{
		UAV_TABLE_VOXELIZE,
#if	USE_MUTEX
		UAV_TABLE_VOXELIZE_X,
		UAV_TABLE_VOXELIZE_Y,
		UAV_TABLE_VOXELIZE_Z,
		UAV_TABLE_MUTEX,
#endif
		UAV_TABLE_KBUFFER,

		NUM_UAV_TABLE
	};

	enum VertexShaderID : uint8_t
	{
		VS_TRI_PROJ,
		VS_TRI_PROJ_TESS,
		VS_TRI_PROJ_UNION,
		VS_BOX_ARRAY,
		VS_SCREEN_QUAD
	};

	enum HullShaderID : uint8_t
	{
		HS_TRI_PROJ
	};

	enum DomainShaderID : uint8_t
	{
		DS_TRI_PROJ
	};

	enum PixelShaderID : uint8_t
	{
		PS_TRI_PROJ,
		PS_TRI_PROJ_SOLID,
		PS_TRI_PROJ_UNION,
		PS_TRI_PROJ_UNION_SOLID,
		PS_SIMPLE,
		PS_RAY_CAST
	};

	enum ComputeShaderID : uint8_t
	{
		CS_FILL_SOLID
	};

	bool createShaders();
	bool createVB(XUSG::CommandList* pCommandList, uint32_t numVert, uint32_t stride,
		const uint8_t* pData, std::vector<XUSG::Resource::uptr>& uploaders);
	bool createIB(XUSG::CommandList* pCommandList, uint32_t numIndices,
		const uint32_t* pData, std::vector<XUSG::Resource::uptr>& uploaders);
	bool createCBs(XUSG::CommandList* pCommandList, std::vector<XUSG::Resource::uptr>& uploaders);
	bool createInputLayout();
	bool prevoxelize(uint8_t mipLevel = 0);
	bool prerenderBoxArray(XUSG::Format rtFormat, XUSG::Format dsFormat);
	bool prerayCast(XUSG::Format rtFormat, XUSG::Format dsFormat);
	void voxelize(XUSG::CommandList* pCommandList, Method voxMethod, bool depthPeel = false, uint8_t mipLevel = 0);
	void voxelizeSolid(XUSG::CommandList* pCommandList, Method voxMethod, uint8_t mipLevel = 0);
	void renderBoxArray(XUSG::CommandList* pCommandList, uint8_t frameIndex,
		const XUSG::Descriptor& rtv, const XUSG::Descriptor& dsv);
	void renderRayCast(XUSG::CommandList* pCommandList, uint8_t frameIndex,
		const XUSG::Descriptor& rtv, const XUSG::Descriptor& dsv);

	XUSG::ShaderLib::uptr				m_shaderLib;
	XUSG::Graphics::PipelineLib::uptr	m_graphicsPipelineLib;
	XUSG::Compute::PipelineLib::uptr	m_computePipelineLib;
	XUSG::PipelineLayoutLib::uptr		m_pipelineLayoutLib;
	XUSG::DescriptorTableLib::sptr		m_descriptorTableLib;

	const XUSG::InputLayout* m_pInputLayout;
	XUSG::PipelineLayout	m_pipelineLayouts[NUM_PASS];
	XUSG::Pipeline			m_pipelines[NUM_PASS];

	XUSG::DescriptorTable	m_cbvTables[NUM_CBV_TABLE];
	XUSG::DescriptorTable	m_srvTables[NUM_SRV_TABLE];
	XUSG::DescriptorTable	m_uavTables[NUM_UAV_TABLE];

	XUSG::VertexBuffer::uptr m_vertexBuffer;
	XUSG::IndexBuffer::uptr	m_indexbuffer;

	XUSG::ConstantBuffer::uptr	m_cbMatrices;
	XUSG::ConstantBuffer::uptr	m_cbPerFrame;
	XUSG::ConstantBuffer::uptr	m_cbPerObject;
	XUSG::ConstantBuffer::uptr	m_cbBound;
	std::vector<XUSG::ConstantBuffer::uptr> m_cbPerMipLevels;

#if	USE_MUTEX
	XUSG::Texture3D::uptr	m_grid[4];
	XUSG::Texture3D::uptr	m_mutex;
#else
	XUSG::Texture3D::uptr	m_grid;
#endif
	XUSG::Texture2D::uptr	m_KBufferDepth;

	DirectX::XMFLOAT4		m_bound;
	DirectX::XMFLOAT2		m_viewport;
	DirectX::XMFLOAT4		m_posScale;

	uint32_t				m_numLevels;
	uint32_t				m_numIndices;
};
