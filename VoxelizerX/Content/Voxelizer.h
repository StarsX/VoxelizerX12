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

	Voxelizer(const XUSG::Device& device);
	virtual ~Voxelizer();

	bool Init(XUSG::CommandList* pCommandList, uint32_t width, uint32_t height, XUSG::Format rtFormat,
		XUSG::Format dsFormat, std::vector<XUSG::Resource>& uploaders, const char* fileName,
		const DirectX::XMFLOAT4& posScale);
	void UpdateFrame(uint32_t frameIndex, DirectX::CXMVECTOR eyePt, DirectX::CXMMATRIX viewProj);
	void Render(const XUSG::CommandList* pCommandList, bool solid, Method voxMethod, uint32_t frameIndex,
		const XUSG::Descriptor& rtv, const XUSG::Descriptor& dsv);

	static const uint32_t FrameCount = FRAME_COUNT;

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
		SRV_TABLE_GRID = SRV_K_DEPTH + FrameCount,

		NUM_SRV_TABLE = SRV_TABLE_GRID + FrameCount
	};

	enum UAVTable : uint8_t
	{
		UAV_TABLE_VOXELIZE,
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

	struct CBMatrices
	{
		DirectX::XMMATRIX worldViewProj;
		DirectX::XMMATRIX world;
		DirectX::XMMATRIX worldIT;
	};

	struct CBPerFrame
	{
		DirectX::XMFLOAT4 eyePos;
	};

	struct CBPerObject
	{
		DirectX::XMVECTOR localSpaceLightPt;
		DirectX::XMVECTOR localSpaceEyePt;
		DirectX::XMMATRIX screenToLocal;
	};

	bool createShaders();
	bool createVB(XUSG::CommandList* pCommandList, uint32_t numVert, uint32_t stride,
		const uint8_t* pData, std::vector<XUSG::Resource>& uploaders);
	bool createIB(XUSG::CommandList* pCommandList, uint32_t numIndices,
		const uint32_t* pData, std::vector<XUSG::Resource>& uploaders);
	bool createCBs(XUSG::CommandList* pCommandList, std::vector<XUSG::Resource>& uploaders);
	bool createInputLayout();
	bool prevoxelize(uint8_t mipLevel = 0);
	bool prerenderBoxArray(XUSG::Format rtFormat, XUSG::Format dsFormat);
	bool prerayCast(XUSG::Format rtFormat, XUSG::Format dsFormat);
	void voxelize(const XUSG::CommandList* pCommandList, Method voxMethod, uint32_t frameIndex,
		bool depthPeel = false, uint8_t mipLevel = 0);
	void voxelizeSolid(const XUSG::CommandList* pCommandList, Method voxMethod,
		uint32_t frameIndex, uint8_t mipLevel = 0);
	void renderBoxArray(const XUSG::CommandList* pCommandList, uint32_t frameIndex,
		const XUSG::Descriptor& rtv, const XUSG::Descriptor& dsv);
	void renderRayCast(const XUSG::CommandList* pCommandList, uint32_t frameIndex,
		const XUSG::Descriptor& rtv, const XUSG::Descriptor& dsv);

	XUSG::Device m_device;

	XUSG::ShaderPool::uptr m_shaderPool;
	XUSG::Graphics::PipelineCache::uptr	m_graphicsPipelineCache;
	XUSG::Compute::PipelineCache::uptr	m_computePipelineCache;
	XUSG::PipelineLayoutCache::uptr		m_pipelineLayoutCache;
	XUSG::DescriptorTableCache::uptr	m_descriptorTableCache;

	const XUSG::InputLayout* m_pInputLayout;
	XUSG::PipelineLayout	m_pipelineLayouts[NUM_PASS];
	XUSG::Pipeline			m_pipelines[NUM_PASS];

	XUSG::DescriptorTable	m_cbvTables[NUM_CBV_TABLE];
	XUSG::DescriptorTable	m_srvTables[NUM_SRV_TABLE];
	XUSG::DescriptorTable	m_uavTables[FrameCount][NUM_UAV_TABLE];
	XUSG::DescriptorTable	m_samplerTable;

	XUSG::VertexBuffer::uptr m_vertexBuffer;
	XUSG::IndexBuffer::uptr	m_indexbuffer;

	XUSG::ConstantBuffer::uptr	m_cbMatrices;
	XUSG::ConstantBuffer::uptr	m_cbPerFrame;
	XUSG::ConstantBuffer::uptr	m_cbPerObject;
	XUSG::ConstantBuffer::uptr	m_cbBound;
	std::vector<XUSG::ConstantBuffer::uptr> m_cbPerMipLevels;

	XUSG::Texture3D::uptr	m_grids[FrameCount];
	XUSG::Texture2D::uptr	m_KBufferDepths[FrameCount];

	DirectX::XMFLOAT4		m_bound;
	DirectX::XMFLOAT2		m_viewport;
	DirectX::XMFLOAT4		m_posScale;

	uint32_t				m_numLevels;
	uint32_t				m_numIndices;
};
