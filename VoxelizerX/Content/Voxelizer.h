#pragma once

#include "DXFramework.h"
#include "Core/XUSG.h"
#include "SharedConst.h"

class Voxelizer
{
public:
	Voxelizer(const XUSG::Device &device, const XUSG::CommandList &commandList);
	virtual ~Voxelizer();

	bool Init(uint32_t width, uint32_t height, XUSG::Format rtFormat, XUSG::Format dsFormat,
		XUSG::Resource &vbUpload, XUSG::Resource &ibUpload,
		const char *fileName = "Media\\bunny.obj");
	void UpdateFrame(uint32_t frameIndex, DirectX::CXMVECTOR eyePt, DirectX::CXMMATRIX viewProj);
	void Render(uint32_t frameIndex, const XUSG::RenderTargetTable &rtvs, const XUSG::Descriptor &dsv);
	void RenderSolid(uint32_t frameIndex, const XUSG::RenderTargetTable &rtvs, const XUSG::Descriptor &dsv);

	static const uint32_t FrameCount = FRAME_COUNT;

protected:
	enum RenderPass : uint8_t
	{
		PASS_VOXELIZE,
		PASS_VOXELIZE_SOLID,
		PASS_FILL_SOLID,
		PASS_DRAW_AS_BOX,
		PASS_RAY_CAST,

		NUM_PASS
	};

	enum CBVTable
	{
		CBV_TABLE_VOXELIZE,
		CBV_TABLE_PER_MIP,
		CBV_TABLE_MATRICES,
		CBV_TABLE_PER_OBJ = CBV_TABLE_MATRICES + FrameCount,

		NUM_CBV_TABLE = CBV_TABLE_PER_OBJ + FrameCount
	};

	enum SRVTable
	{
		SRV_TABLE_VB_IB,
		SRV_K_DEPTH,
		SRV_TABLE_GRID = SRV_K_DEPTH + FrameCount,

		NUM_SRV_TABLE = SRV_TABLE_GRID + FrameCount
	};

	enum UAVTable
	{
		UAV_TABLE_VOXELIZE,
		UAV_TABLE_KBUFFER,

		NUM_UAV_TABLE
	};

	enum VertexShaderID : uint8_t
	{
		VS_TRI_PROJ,
		VS_BOX_ARRAY,
		VS_SCREEN_QUAD
	};

	enum PixelShaderID : uint8_t
	{
		PS_TRI_PROJ,
		PS_TRI_PROJ_SOLID,
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
	bool createVB(uint32_t numVert, uint32_t stride, const uint8_t *pData, XUSG::Resource &vbUpload);
	bool createIB(uint32_t numIndices, const uint32_t *pData, XUSG::Resource &ibUpload);
	bool createCBs();
	void createInputLayout();
	bool prevoxelize(uint8_t mipLevel = 0);
	bool prerenderBoxArray(XUSG::Format rtFormat, XUSG::Format dsFormat);
	bool prerayCast(XUSG::Format rtFormat, XUSG::Format dsFormat);
	void voxelize(uint32_t frameIndex, bool depthPeel = false, uint8_t mipLevel = 0);
	void voxelizeSolid(uint32_t frameIndex, uint8_t mipLevel = 0);
	void renderBoxArray(uint32_t frameIndex, const XUSG::RenderTargetTable &rtvs, const XUSG::Descriptor &dsv);
	void renderRayCast(uint32_t frameIndex, const XUSG::RenderTargetTable &rtvs, const XUSG::Descriptor &dsv);

	XUSG::Device m_device;
	XUSG::CommandList m_commandList;

	XUSG::ShaderPool				m_shaderPool;
	XUSG::Graphics::PipelineCache	m_graphicsPipelineCache;
	XUSG::Compute::PipelineCache	m_computePipelineCache;
	XUSG::PipelineLayoutCache		m_pipelineLayoutCache;
	XUSG::DescriptorTableCache		m_descriptorTableCache;

	XUSG::InputLayout		m_inputLayout;
	XUSG::PipelineLayout	m_pipelineLayouts[NUM_PASS];
	XUSG::Pipeline			m_pipelines[NUM_PASS];

	XUSG::DescriptorTable	m_cbvTables[NUM_CBV_TABLE];
	XUSG::DescriptorTable	m_srvTables[NUM_SRV_TABLE];
	XUSG::DescriptorTable	m_uavTables[FrameCount][NUM_UAV_TABLE];
	XUSG::DescriptorTable	m_samplerTable;

	XUSG::VertexBuffer		m_vertexBuffer;
	XUSG::IndexBuffer		m_indexbuffer;

	XUSG::ConstantBuffer	m_cbMatrices;
	XUSG::ConstantBuffer	m_cbPerFrame;
	XUSG::ConstantBuffer	m_cbPerObject;
	XUSG::ConstantBuffer	m_cbBound;
	std::vector<XUSG::ConstantBuffer> m_cbPerMipLevels;

	XUSG::Texture3D			m_grids[FrameCount];
	XUSG::Texture2D			m_KBufferDepths[FrameCount];

	DirectX::XMFLOAT4		m_bound;
	DirectX::XMFLOAT2		m_viewport;
	DirectX::XMUINT2		m_windowSize;

	uint32_t				m_numLevels;
	uint32_t				m_numIndices;
};
