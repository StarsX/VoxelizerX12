#include "ObjLoader.h"
#include "Voxelizer.h"

using namespace std;
using namespace DirectX;
using namespace XUSG;

Voxelizer::Voxelizer(const Device& device, const GraphicsCommandList& commandList,
	const ComPtr<ID3D12CommandQueue>& commandQueue) :
	m_device(device),
	m_commandList(commandList),
	m_commandQueue(commandQueue),
	m_vertexStride(0),
	m_numIndices(0)
{
}

Voxelizer::~Voxelizer()
{
}

void Voxelizer::Init(uint32_t width, uint32_t height, Resource& vbUpload, Resource& ibUpload,
	const char *fileName)
{
	m_viewport.x = static_cast<float>(width);
	m_viewport.y = static_cast<float>(height);

	ObjLoader objLoader;
	if (!objLoader.Import(fileName, true, true)) return;

	createInputLayout();
	createVB(objLoader.GetNumVertices(), objLoader.GetVertexStride(), objLoader.GetVertices(), vbUpload);
	createIB(objLoader.GetNumIndices(), objLoader.GetIndices(), ibUpload);

	// Extract boundary
	const auto venter = objLoader.GetCenter();
	m_bound = XMFLOAT4(venter.x, venter.y, venter.z, objLoader.GetRadius());

	m_numLevels = max(static_cast<uint32_t>(log2(GRID_SIZE)), 1);
	createCBs();

	m_grid = make_unique<Texture3D>(m_device);
	m_grid->Create(GRID_SIZE, GRID_SIZE, GRID_SIZE, DXGI_FORMAT_R10G10B10A2_UNORM, BIND_PACKED_UAV);

	m_KBufferDepth = make_unique<Texture2D>(m_device);
	m_KBufferDepth->Create(GRID_SIZE, GRID_SIZE, DXGI_FORMAT_R32_UINT, static_cast<uint32_t>(GRID_SIZE * DEPTH_SCALE),
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
}

void Voxelizer::UpdateFrame(CXMVECTOR eyePt, CXMMATRIX viewProj)
{
	// General matrices
	const auto world = XMMatrixScaling(m_bound.w, m_bound.w, m_bound.w) *
		XMMatrixTranslation(m_bound.x, m_bound.y, m_bound.z);
	const auto worldI = XMMatrixInverse(nullptr, world);
	const auto worldViewProj = world * viewProj;
	const auto pCbMatrices = reinterpret_cast<CBMatrices*>(m_cbMatrices->Map());
	pCbMatrices->worldViewProj = XMMatrixTranspose(worldViewProj);
	pCbMatrices->world = world;
	pCbMatrices->worldIT = XMMatrixIdentity();
	
	// Screen space matrices
	const auto pCbPerObject = reinterpret_cast<CBPerObject*>(m_cbPerObject->Map());
	pCbPerObject->localSpaceLightPt = XMVector3TransformCoord(XMVectorSet(10.0f, 45.0f, 75.0f, 0.0f), worldI);
	pCbPerObject->localSpaceEyePt = XMVector3TransformCoord(eyePt, worldI);

	const auto mToScreen = XMMATRIX
	(
		0.5f * m_viewport.x, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f * m_viewport.y, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f * m_viewport.x, 0.5f * m_viewport.y, 0.0f, 1.0f
	);
	const auto localToScreen = XMMatrixMultiply(worldViewProj, mToScreen);
	const auto screenToLocal = XMMatrixInverse(nullptr, localToScreen);
	pCbPerObject->screenToLocal = XMMatrixTranspose(screenToLocal);

	// Per-frame data
	const auto pCbPerFrame = reinterpret_cast<CBPerFrame*>(m_cbPerFrame->Map());
	XMStoreFloat4(&pCbPerFrame->eyePos, eyePt);
}

void Voxelizer::createInputLayout()
{
	const auto offset = D3D12_APPEND_ALIGNED_ELEMENT;

	// Define the vertex input layout.
	InputElementTable inputElementDescs =
	{
		{ "POSITION",	0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL",		0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offset,	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	m_inputLayout = m_pipelinePool.CreateInputLayout(inputElementDescs);
}

void Voxelizer::createVB(uint32_t numVert, uint32_t stride, const uint8_t *pData, Resource& vbUpload)
{
	m_vertexStride = stride;
	m_vertexBuffer = make_unique<RawBuffer>(m_device);
	m_vertexBuffer->Create(stride * numVert, D3D12_RESOURCE_FLAG_NONE, D3D12_HEAP_TYPE_DEFAULT,
		D3D12_RESOURCE_STATE_COPY_DEST);
	m_vertexBuffer->Upload(m_commandList, vbUpload, pData, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}

void Voxelizer::createIB(uint32_t numIndices, const uint32_t *pData, Resource& ibUpload)
{
	m_numIndices = numIndices;
	m_indexbuffer = make_unique<RawBuffer>(m_device);
	m_indexbuffer->Create(sizeof(uint32_t) * numIndices, D3D12_RESOURCE_FLAG_NONE, D3D12_HEAP_TYPE_DEFAULT,
		D3D12_RESOURCE_STATE_COPY_DEST);
	m_indexbuffer->Upload(m_commandList, ibUpload, pData, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}

void Voxelizer::createCBs()
{
	// Common CBs
	{
		m_cbMatrices = make_unique<ConstantBuffer>(m_device);
		m_cbMatrices->Create(sizeof(XMMATRIX) * 4 * 32, sizeof(CBMatrices));
		
		m_cbPerFrame = make_unique<ConstantBuffer>(m_device);
		m_cbPerFrame->Create(sizeof(XMFLOAT4) * 512, sizeof(CBPerFrame));

		m_cbPerObject = make_unique<ConstantBuffer>(m_device);
		m_cbPerObject->Create(sizeof(XMMATRIX) * 2 * 64, sizeof(CBPerObject));
	}

	// Immutable CBs
	{
		m_cbBound = make_unique<ConstantBuffer>(m_device);
		m_cbBound->Create(256, sizeof(XMFLOAT4));

		const auto pCbBound = reinterpret_cast<XMFLOAT4*>(m_cbBound->Map());
		*pCbBound = m_bound;
		m_cbBound->Unmap();
	}

	m_cbPerMipLevels.resize(m_numLevels);
	for (auto i = 0u; i < m_numLevels; ++i)
	{
		auto& cb = m_cbPerMipLevels[i];
		const auto gridSize = static_cast<float>(GRID_SIZE >> i);
		cb = make_unique<ConstantBuffer>(m_device);
		cb->Create(256, sizeof(XMFLOAT4));

		const auto pCbData = reinterpret_cast<float*>(cb->Map());
		*pCbData = gridSize;
		cb->Unmap();
	}
}
