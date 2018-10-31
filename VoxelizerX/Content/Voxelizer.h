#pragma once

#include "DXFramework.h"
#include "Core/XUSGGraphicsState.h"
#include "Core/XUSGResource.h"

#include "SharedConst.h"

class Voxelizer
{
public:
	enum VertexShaderID : uint32_t
	{
		VS_TRI_PROJ,
		VS_BOX_ARRAY
	};

	enum PixelShaderID : uint32_t
	{
		PS_TRI_PROJ,
		PS_TRI_PROJ_SOLID,
		PS_SIMPLE
	};

	enum ComputeShaderID : uint32_t
	{
		CS_FILL_SOLID,
		CS_RAY_CAST
	};

	Voxelizer(const XUSG::Device& device, const XUSG::GraphicsCommandList& commandList,
		const ComPtr<ID3D12CommandQueue>& commandQueue);
	virtual ~Voxelizer();

	void Init(uint32_t width, uint32_t height, XUSG::Resource& vbUpload, XUSG::Resource& ibUpload,
		const char *fileName = "Media\\bunny.obj");
	void UpdateFrame(DirectX::CXMVECTOR eyePt, DirectX::CXMMATRIX viewProj);
	void Render();

protected:
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

	void createShaders();
	void createInputLayout();
	void createVB(uint32_t numVert, uint32_t stride, const uint8_t *pData, XUSG::Resource& vbUpload);
	void createIB(uint32_t numIndices, const uint32_t *pData, XUSG::Resource& ibUpload);
	void createCBs();

	uint32_t	m_vertexStride;
	uint32_t	m_numIndices;

	uint32_t	m_numLevels;

	DirectX::XMFLOAT4	m_bound;
	DirectX::XMFLOAT2	m_viewport;

	XUSG::InputLayout m_inputLayout;

	std::unique_ptr<XUSG::RawBuffer> m_vertexBuffer;
	std::unique_ptr<XUSG::RawBuffer> m_indexbuffer;

	std::unique_ptr<XUSG::ConstantBuffer> m_cbMatrices;
	std::unique_ptr<XUSG::ConstantBuffer> m_cbPerFrame;
	std::unique_ptr<XUSG::ConstantBuffer> m_cbPerObject;
	std::unique_ptr<XUSG::ConstantBuffer> m_cbBound;
	std::vector<std::unique_ptr<XUSG::ConstantBuffer>> m_cbPerMipLevels;

	std::unique_ptr<XUSG::Texture3D> m_grid;
	std::unique_ptr<XUSG::Texture2D> m_KBufferDepth;

	XUSG::Shader::Pool				m_shaderPool;
	XUSG::Graphics::Pipeline::Pool	m_pipelinePool;
	XUSG::DescriptorTablePool		m_descriptorTablePool;

	XUSG::Device m_device;
	XUSG::GraphicsCommandList m_commandList;
	ComPtr<ID3D12CommandQueue> m_commandQueue;
};
