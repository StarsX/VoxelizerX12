#include "ObjLoader.h"
#include "Voxelizer.h"

using namespace std;
using namespace DirectX;
using namespace XUSG;

Voxelizer::Voxelizer(const Device &device, const GraphicsCommandList &commandList) :
	m_device(device),
	m_commandList(commandList),
	m_vertexStride(0),
	m_numIndices(0)
{
	m_pipelinePool.SetDevice(device);
	m_descriptorTablePool.SetDevice(device);
}

Voxelizer::~Voxelizer()
{
}

void Voxelizer::Init(uint32_t width, uint32_t height, Format rtFormat, Format dsFormat,
	Resource &vbUpload, Resource &ibUpload, const char *fileName)
{
	m_viewport.x = static_cast<float>(width);
	m_viewport.y = static_cast<float>(height);

	// Create shaders
	createShaders();

	// Load inputs
	ObjLoader objLoader;
	if (!objLoader.Import(fileName, true, true)) return;

	//createInputLayout();
	createVB(objLoader.GetNumVertices(), objLoader.GetVertexStride(), objLoader.GetVertices(), vbUpload);
	createIB(objLoader.GetNumIndices(), objLoader.GetIndices(), ibUpload);

	// Extract boundary
	const auto center = objLoader.GetCenter();
	m_bound = XMFLOAT4(center.x, center.y, center.z, objLoader.GetRadius());

	m_numLevels = max(static_cast<uint32_t>(log2(GRID_SIZE)), 1);
	createCBs();

	for (auto &grid : m_grids)
		grid.Create(m_device, GRID_SIZE, GRID_SIZE, GRID_SIZE, DXGI_FORMAT_R10G10B10A2_UNORM, BIND_PACKED_UAV);

	for (auto &KBufferDepth : m_KBufferDepths)
		KBufferDepth.Create(m_device, GRID_SIZE, GRID_SIZE, DXGI_FORMAT_R32_UINT, static_cast<uint32_t>(GRID_SIZE * DEPTH_SCALE),
			D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	// Prepare for rendering
	prevoxelize();
	prerenderBoxArray(rtFormat, dsFormat);
}

void Voxelizer::UpdateFrame(CXMVECTOR eyePt, CXMMATRIX viewProj)
{
	// General matrices
	const auto world = XMMatrixScaling(m_bound.w, m_bound.w, m_bound.w) *
		XMMatrixTranslation(m_bound.x, m_bound.y, m_bound.z);
	const auto worldI = XMMatrixInverse(nullptr, world);
	const auto worldViewProj = world * viewProj;
	const auto pCbMatrices = reinterpret_cast<CBMatrices*>(m_cbMatrices.Map());
	pCbMatrices->worldViewProj = XMMatrixTranspose(worldViewProj);
	pCbMatrices->world = world;
	pCbMatrices->worldIT = XMMatrixIdentity();
	
	// Screen space matrices
	const auto pCbPerObject = reinterpret_cast<CBPerObject*>(m_cbPerObject.Map());
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
	const auto pCbPerFrame = reinterpret_cast<CBPerFrame*>(m_cbPerFrame.Map());
	XMStoreFloat4(&pCbPerFrame->eyePos, eyePt);
}

void Voxelizer::Render(uint32_t frameIndex, const RenderTargetTable &rtvs, const Descriptor &dsv)
{
	m_commandList->SetDescriptorHeaps(1, m_descriptorTablePool.GetCbvSrvUavPool().GetAddressOf());

	voxelize(frameIndex);
	renderBoxArray(frameIndex, rtvs, dsv);
}

void Voxelizer::createShaders()
{
	m_shaderPool.CreateShader(Shader::Stage::VS, VS_TRI_PROJ, L"VSTriProj.cso");
	m_shaderPool.CreateShader(Shader::Stage::VS, VS_BOX_ARRAY, L"VSBoxArray.cso");

	m_shaderPool.CreateShader(Shader::Stage::PS, PS_TRI_PROJ, L"PSTriProj.cso");
	m_shaderPool.CreateShader(Shader::Stage::PS, PS_SIMPLE, L"PSSimple.cso");
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

void Voxelizer::createVB(uint32_t numVert, uint32_t stride, const uint8_t *pData, Resource &vbUpload)
{
	m_vertexStride = stride;
	m_vertexBuffer.Create(m_device, stride * numVert, stride, D3D12_RESOURCE_FLAG_NONE,
		D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COPY_DEST);
	m_vertexBuffer.Upload(m_commandList, vbUpload, pData, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}

void Voxelizer::createIB(uint32_t numIndices, const uint32_t *pData, Resource &ibUpload)
{
	m_numIndices = numIndices;
	m_indexbuffer.Create(m_device, sizeof(uint32_t) * numIndices, DXGI_FORMAT_R32_UINT, D3D12_RESOURCE_FLAG_NONE,
		D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COPY_DEST);
	m_indexbuffer.Upload(m_commandList, ibUpload, pData, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}

void Voxelizer::createCBs()
{
	// Common CBs
	{
		m_cbMatrices.Create(m_device, ((sizeof(CBMatrices) + 255) & ~255) * 8, sizeof(CBMatrices));
		m_cbPerFrame.Create(m_device, ((sizeof(CBPerFrame) + 255) & ~255) * 8, sizeof(CBPerFrame));
		m_cbPerObject.Create(m_device, ((sizeof(CBPerObject) + 255) & ~255) * 8, sizeof(CBPerObject));
	}

	// Immutable CBs
	{
		m_cbBound.Create(m_device, 256, sizeof(XMFLOAT4));

		const auto pCbBound = reinterpret_cast<XMFLOAT4*>(m_cbBound.Map());
		*pCbBound = m_bound;
		m_cbBound.Unmap();
	}

	m_cbPerMipLevels.resize(m_numLevels);
	for (auto i = 0u; i < m_numLevels; ++i)
	{
		auto &cb = m_cbPerMipLevels[i];
		const auto gridSize = static_cast<float>(GRID_SIZE >> i);
		cb.Create(m_device, 256, sizeof(XMFLOAT4));

		const auto pCbData = reinterpret_cast<float*>(cb.Map());
		*pCbData = gridSize;
		cb.Unmap();
	}
}

void Voxelizer::prevoxelize(uint8_t mipLevel)
{
	// Get CBVs
	Util::DescriptorTable utilCbvTable;
	utilCbvTable.SetDescriptors(0, 1, &m_cbBound.GetCBV());
	utilCbvTable.SetDescriptors(1, 1, &m_cbPerMipLevels[mipLevel].GetCBV());
	m_cbvTables[CBV_TABLE_VOXELIZE] = utilCbvTable.GetCbvSrvUavTable(m_descriptorTablePool);

	Util::DescriptorTable utilCbvPerMipLevelTable;
	utilCbvPerMipLevelTable.SetDescriptors(0, 1, &m_cbPerMipLevels[mipLevel].GetCBV());
	m_cbvTables[CBV_TABLE_PER_MIP] = utilCbvPerMipLevelTable.GetCbvSrvUavTable(m_descriptorTablePool);

	// Get SRVs
	const Descriptor srvs[] = { m_indexbuffer.GetSRV(), m_vertexBuffer.GetSRV() };
	Util::DescriptorTable utilSrvTable;
	utilSrvTable.SetDescriptors(0, _countof(srvs), srvs);
	m_srvTables[SRV_TABLE_VB_IB] = utilSrvTable.GetCbvSrvUavTable(m_descriptorTablePool);

	// Get UAVs
	for (auto i = 0ui8; i < FrameCount; ++i)
	{
		Util::DescriptorTable utilUavGridTable;
		utilUavGridTable.SetDescriptors(0, 1, &m_grids[i].GetUAV());
		m_uavTables[UAV_TABLE_VOXELIZE][i] = utilUavGridTable.GetCbvSrvUavTable(m_descriptorTablePool);

		Util::DescriptorTable utilUavKBufferTable;
		utilUavKBufferTable.SetDescriptors(0, 1, &m_KBufferDepths[i].GetUAV());
		m_uavTables[UAV_TABLE_KBUFFER][i] = utilUavKBufferTable.GetCbvSrvUavTable(m_descriptorTablePool);
	}

	// Get pipeline layout
	Util::PipelineLayout utilPipelineLayout;
	utilPipelineLayout.SetRange(0, DescriptorType::CBV, 2, 0);
	utilPipelineLayout.SetRange(1, DescriptorType::CBV, 1, 0);
	utilPipelineLayout.SetRange(2, DescriptorType::SRV, _countof(srvs), 0);
	utilPipelineLayout.SetRange(3, DescriptorType::UAV, 1, 0);
	//utilPipelineLayout.SetRange(3, DescriptorType::UAV, depthPeel ? 2 : 1, 0);
	utilPipelineLayout.SetShaderStage(0, Shader::Stage::VS);
	utilPipelineLayout.SetShaderStage(1, Shader::Stage::PS);
	utilPipelineLayout.SetShaderStage(2, Shader::Stage::VS);
	utilPipelineLayout.SetShaderStage(3, Shader::Stage::PS);
	m_pipelineLayouts[PASS_VOXELIZE] = utilPipelineLayout.GetPipelineLayout(m_pipelinePool, D3D12_ROOT_SIGNATURE_FLAG_NONE);

	// Get pipeline
	Graphics::State state;
	state.SetPipelineLayout(m_pipelineLayouts[PASS_VOXELIZE]);
	state.SetShader(Shader::Stage::VS, m_shaderPool.GetShader(Shader::Stage::VS, VS_TRI_PROJ));
	state.SetShader(Shader::Stage::PS, m_shaderPool.GetShader(Shader::Stage::PS, PS_TRI_PROJ));
	state.DSSetState(Graphics::DepthStencilPreset::DEPTH_STENCIL_NONE, m_pipelinePool);
	state.IASetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	state.RSSetState(Graphics::RasterizerPreset::CULL_NONE, m_pipelinePool);
	state.OMSetNumRenderTargets(0);
	//state.OMSetRTVFormat(0, DXGI_FORMAT_R8G8B8A8_UNORM);
	m_pipelines[PASS_VOXELIZE] = state.GetPipeline(m_pipelinePool);
}

void Voxelizer::prerenderBoxArray(Format rtFormat, Format dsFormat)
{
	// Get CBVs
	Util::DescriptorTable utilCbvTable;
	utilCbvTable.SetDescriptors(0, 1, &m_cbMatrices.GetCBV());
	m_cbvTables[CBV_TABLE_MATRICES] = utilCbvTable.GetCbvSrvUavTable(m_descriptorTablePool);

	// Get SRV
	for (auto i = 0ui8; i < FrameCount; ++i)
	{
		Util::DescriptorTable utilSrvTable;
		utilSrvTable.SetDescriptors(0, 1, &m_grids[i].GetSRV());
		m_srvTables[SRV_TABLE_GRID + i] = utilSrvTable.GetCbvSrvUavTable(m_descriptorTablePool);
	}

	// Get pipeline layout
	Util::PipelineLayout utilPipelineLayout;
	utilPipelineLayout.SetRange(0, DescriptorType::CBV, 1, 0);
	utilPipelineLayout.SetRange(1, DescriptorType::SRV, 1, 0);
	utilPipelineLayout.SetShaderStage(0, Shader::Stage::VS);
	utilPipelineLayout.SetShaderStage(1, Shader::Stage::VS);
	m_pipelineLayouts[PASS_DRAW_AS_BOX] = utilPipelineLayout.GetPipelineLayout(m_pipelinePool, D3D12_ROOT_SIGNATURE_FLAG_NONE);

	// Get pipeline
	Graphics::State state;
	state.SetPipelineLayout(m_pipelineLayouts[PASS_DRAW_AS_BOX]);
	state.SetShader(Shader::Stage::VS, m_shaderPool.GetShader(Shader::Stage::VS, VS_BOX_ARRAY));
	state.SetShader(Shader::Stage::PS, m_shaderPool.GetShader(Shader::Stage::PS, PS_SIMPLE));
	state.IASetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	state.OMSetNumRenderTargets(1);
	state.OMSetRTVFormat(0, rtFormat);
	state.OMSetDSVFormat(dsFormat);
	m_pipelines[PASS_DRAW_AS_BOX] = state.GetPipeline(m_pipelinePool);
}

void Voxelizer::voxelize(uint32_t frameIndex, bool depthPeel, uint8_t mipLevel)
{
	// Set pipeline state
	m_commandList->SetPipelineState(m_pipelines[PASS_VOXELIZE].Get());
	m_commandList->SetGraphicsRootSignature(m_pipelineLayouts[PASS_VOXELIZE].Get());

	// Set descriptor tables
	m_grids[frameIndex].Barrier(m_commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	m_commandList->SetGraphicsRootDescriptorTable(0, *m_cbvTables[CBV_TABLE_VOXELIZE]);
	m_commandList->SetGraphicsRootDescriptorTable(1, *m_cbvTables[CBV_TABLE_PER_MIP]);
	m_commandList->SetGraphicsRootDescriptorTable(2, *m_srvTables[SRV_TABLE_VB_IB]);
	m_commandList->SetGraphicsRootDescriptorTable(3, *m_uavTables[UAV_TABLE_VOXELIZE][frameIndex]);

	// Set viewport
	const auto gridSize = GRID_SIZE >> mipLevel;
	const auto fGridSize = static_cast<float>(gridSize);
	CD3DX12_VIEWPORT viewport(0.0f, 0.0f, fGridSize, fGridSize);
	CD3DX12_RECT scissorRect(0, 0, gridSize, gridSize);
	m_commandList->RSSetViewports(1, &viewport);
	m_commandList->RSSetScissorRects(1, &scissorRect);

	// Record commands.
	m_commandList->ClearUnorderedAccessViewUint(*m_uavTables[UAV_TABLE_VOXELIZE][frameIndex], m_grids[frameIndex].GetUAV(),
		m_grids[frameIndex].GetResource().Get(), XMVECTORU32{ 0 }.u, 0, nullptr);
	if (depthPeel) m_commandList->ClearUnorderedAccessViewUint(*m_uavTables[UAV_TABLE_KBUFFER][frameIndex], m_KBufferDepths[frameIndex].GetUAV(),
		m_KBufferDepths[frameIndex].GetResource().Get(), XMVECTORU32{ UINT32_MAX }.u, 0, nullptr);

	m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_commandList->DrawInstanced(3, m_numIndices / 3, 0, 0);
}

void Voxelizer::renderBoxArray(uint32_t frameIndex, const RenderTargetTable &rtvs, const Descriptor &dsv)
{
	// Set pipeline state
	m_commandList->SetPipelineState(m_pipelines[PASS_DRAW_AS_BOX].Get());
	m_commandList->SetGraphicsRootSignature(m_pipelineLayouts[PASS_DRAW_AS_BOX].Get());

	// Set descriptor tables
	m_grids[frameIndex].Barrier(m_commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	m_commandList->SetGraphicsRootDescriptorTable(0, *m_cbvTables[CBV_TABLE_MATRICES]);
	m_commandList->SetGraphicsRootDescriptorTable(1, *m_srvTables[SRV_TABLE_GRID + frameIndex]);

	// Set viewport
	const auto gridSize = GRID_SIZE >> SHOW_MIP;
	CD3DX12_VIEWPORT viewport(0.0f, 0.0f, m_viewport.x, m_viewport.y);
	CD3DX12_RECT scissorRect(0, 0, static_cast<long>(m_viewport.x), static_cast<long>(m_viewport.y));
	m_commandList->RSSetViewports(1, &viewport);
	m_commandList->RSSetScissorRects(1, &scissorRect);

	m_commandList->OMSetRenderTargets(1, rtvs.get(), FALSE, &dsv);

	// Record commands.
	m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	m_commandList->DrawInstanced(4, 6 * gridSize * gridSize * gridSize, 0, 0);
}
