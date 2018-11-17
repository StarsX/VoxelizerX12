#pragma once

#include "DXFramework.h"
#include "Core/XUSGGraphicsState.h"
#include "Core/XUSGResource.h"

#include "SharedConst.h"

class Voxelizer
{
public:
	Voxelizer(const XUSG::Device &device, const XUSG::GraphicsCommandList &commandList);
	virtual ~Voxelizer();

	bool Init(uint32_t width, uint32_t height, XUSG::Format rtFormat, XUSG::Format dsFormat,
		XUSG::Resource &vbUpload, XUSG::Resource &ibUpload,
		const char *fileName = "Media\\bunny.obj");
	void UpdateFrame(DirectX::CXMVECTOR eyePt, DirectX::CXMMATRIX viewProj);
	void Render(uint32_t frameIndex, const XUSG::RenderTargetTable &rtvs, const XUSG::Descriptor &dsv);

	static const uint32_t FrameCount = 3;

protected:
	enum RenderPass : uint8_t
	{
		PASS_VOXELIZE,
		PASS_DRAW_AS_BOX,

		NUM_PASS
	};

	enum CBVTable
	{
		CBV_TABLE_VOXELIZE,
		CBV_TABLE_PER_MIP,
		CBV_TABLE_MATRICES,

		NUM_CBV_TABLE
	};

	enum SRVTable
	{
		SRV_TABLE_VB_IB,
		SRV_TABLE_GRID,

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
		VS_BOX_ARRAY
	};

	enum PixelShaderID : uint8_t
	{
		PS_TRI_PROJ,
		PS_TRI_PROJ_SOLID,
		PS_SIMPLE
	};

	enum ComputeShaderID : uint8_t
	{
		CS_FILL_SOLID,
		CS_RAY_CAST
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
	void prevoxelize(uint8_t mipLevel = 0);
	void prerenderBoxArray(XUSG::Format rtFormat, XUSG::Format dsFormat);
	void voxelize(uint32_t frameIndex, bool depthPeel = false, uint8_t mipLevel = 0);
	void renderBoxArray(uint32_t frameIndex, const XUSG::RenderTargetTable &rtvs, const XUSG::Descriptor &dsv);

	uint32_t	m_vertexStride;
	uint32_t	m_numIndices;

	uint32_t	m_numLevels;

	DirectX::XMFLOAT4		m_bound;
	DirectX::XMFLOAT2		m_viewport;

	XUSG::InputLayout		m_inputLayout;
	XUSG::PipelineLayout	m_pipelineLayouts[NUM_PASS];
	XUSG::PipelineState		m_pipelines[NUM_PASS];

	XUSG::DescriptorTable	m_cbvTables[NUM_CBV_TABLE];
	XUSG::DescriptorTable	m_srvTables[NUM_SRV_TABLE];
	XUSG::DescriptorTable	m_uavTables[NUM_UAV_TABLE][FrameCount];

	XUSG::VertexBuffer		m_vertexBuffer;
	XUSG::IndexBuffer		m_indexbuffer;

	XUSG::ConstantBuffer	m_cbMatrices;
	XUSG::ConstantBuffer	m_cbPerFrame;
	XUSG::ConstantBuffer	m_cbPerObject;
	XUSG::ConstantBuffer	m_cbBound;
	std::vector<XUSG::ConstantBuffer> m_cbPerMipLevels;

	XUSG::Texture3D			m_grids[FrameCount];
	XUSG::Texture2D			m_KBufferDepths[FrameCount];

	XUSG::Shader::Pool				m_shaderPool;
	XUSG::Graphics::Pipeline::Pool	m_pipelinePool;
	XUSG::PipelineLayoutPool		m_pipelineLayoutPool;
	XUSG::DescriptorTablePool		m_descriptorTablePool;

	XUSG::Device m_device;
	XUSG::GraphicsCommandList m_commandList;
};
