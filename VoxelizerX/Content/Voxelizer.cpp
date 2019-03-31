#include "ObjLoader.h"
#include "Voxelizer.h"

using namespace std;
using namespace DirectX;
using namespace XUSG;

Voxelizer::Voxelizer(const Device &device, const CommandList &commandList) :
	m_device(device),
	m_commandList(commandList)
{
	m_graphicsPipelineCache.SetDevice(device);
	m_computePipelineCache.SetDevice(device);
	m_descriptorTableCache.SetDevice(device);
	m_pipelineLayoutCache.SetDevice(device);
}

Voxelizer::~Voxelizer()
{
}

bool Voxelizer::Init(uint32_t width, uint32_t height, Format rtFormat, Format dsFormat,
	Resource &vbUpload, Resource &ibUpload, const char *fileName)
{
	m_viewport.x = static_cast<float>(width);
	m_viewport.y = static_cast<float>(height);

	// Create shaders
	N_RETURN(createShaders(), false);

	// Load inputs
	ObjLoader objLoader;
	if (!objLoader.Import(fileName, true, true)) return false;

	createInputLayout();
	N_RETURN(createVB(objLoader.GetNumVertices(), objLoader.GetVertexStride(), objLoader.GetVertices(), vbUpload), false);
	N_RETURN(createIB(objLoader.GetNumIndices(), objLoader.GetIndices(), ibUpload), false);

	// Extract boundary
	const auto center = objLoader.GetCenter();
	m_bound = XMFLOAT4(center.x, center.y, center.z, objLoader.GetRadius());

	m_numLevels = max(static_cast<uint32_t>(log2(GRID_SIZE)), 1);
	N_RETURN(createCBs(), false);

	for (auto &grid : m_grids)
		N_RETURN(grid.Create(m_device, GRID_SIZE, GRID_SIZE, GRID_SIZE, DXGI_FORMAT_R10G10B10A2_UNORM, BIND_PACKED_UAV), false);

	for (auto &KBufferDepth : m_KBufferDepths)
		N_RETURN(KBufferDepth.Create(m_device, GRID_SIZE, GRID_SIZE, DXGI_FORMAT_R32_UINT, static_cast<uint32_t>(GRID_SIZE * DEPTH_SCALE),
			D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS), false);

	// Prepare for rendering
	N_RETURN(prevoxelize(), false);
	N_RETURN(prerenderBoxArray(rtFormat, dsFormat), false);
	N_RETURN(prerayCast(rtFormat, dsFormat), false);

	return true;
}

void Voxelizer::UpdateFrame(uint32_t frameIndex, CXMVECTOR eyePt, CXMMATRIX viewProj)
{
	// General matrices
	const auto world = XMMatrixScaling(m_bound.w, m_bound.w, m_bound.w) *
		XMMatrixTranslation(m_bound.x, m_bound.y, m_bound.z);
	const auto worldI = XMMatrixInverse(nullptr, world);
	const auto worldViewProj = world * viewProj;
	const auto pCbMatrices = reinterpret_cast<CBMatrices*>(m_cbMatrices.Map(frameIndex));
	pCbMatrices->worldViewProj = XMMatrixTranspose(worldViewProj);
	pCbMatrices->world = world;
	pCbMatrices->worldIT = XMMatrixIdentity();
	
	// Screen space matrices
	const auto pCbPerObject = reinterpret_cast<CBPerObject*>(m_cbPerObject.Map(frameIndex));
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
	const auto pCbPerFrame = reinterpret_cast<CBPerFrame*>(m_cbPerFrame.Map(frameIndex));
	XMStoreFloat4(&pCbPerFrame->eyePos, eyePt);
}

void Voxelizer::Render(bool solid, Method voxMethod, uint32_t frameIndex, const RenderTargetTable &rtvs, const Descriptor &dsv)
{
	if (solid)
	{
		const DescriptorPool descriptorPools[] =
		{
			m_descriptorTableCache.GetDescriptorPool(CBV_SRV_UAV_POOL),
			m_descriptorTableCache.GetDescriptorPool(SAMPLER_POOL)
		};
		m_commandList.SetDescriptorPools(static_cast<uint32_t>(size(descriptorPools)), descriptorPools);

		voxelizeSolid(voxMethod, frameIndex);
		renderRayCast(frameIndex, rtvs, dsv);
	}
	else
	{
		const DescriptorPool descriptorPools[] =
		{ m_descriptorTableCache.GetDescriptorPool(CBV_SRV_UAV_POOL) };
		m_commandList.SetDescriptorPools(static_cast<uint32_t>(size(descriptorPools)), descriptorPools);

		voxelize(voxMethod, frameIndex);
		renderBoxArray(frameIndex, rtvs, dsv);
	}
}

bool Voxelizer::createShaders()
{
	N_RETURN(m_shaderPool.CreateShader(Shader::Stage::VS, VS_TRI_PROJ, L"VSTriProj.cso"), false);
	N_RETURN(m_shaderPool.CreateShader(Shader::Stage::VS, VS_TRI_PROJ_TESS, L"VSTriProjTess.cso"), false);
	N_RETURN(m_shaderPool.CreateShader(Shader::Stage::VS, VS_TRI_PROJ_UNION, L"VSTriProjUnion.cso"), false);
	N_RETURN(m_shaderPool.CreateShader(Shader::Stage::VS, VS_BOX_ARRAY, L"VSBoxArray.cso"), false);
	N_RETURN(m_shaderPool.CreateShader(Shader::Stage::VS, VS_SCREEN_QUAD, L"VSScreenQuad.cso"), false);

	N_RETURN(m_shaderPool.CreateShader(Shader::Stage::HS, HS_TRI_PROJ, L"HSTriProj.cso"), false);
	N_RETURN(m_shaderPool.CreateShader(Shader::Stage::DS, DS_TRI_PROJ, L"DSTriProj.cso"), false);

	N_RETURN(m_shaderPool.CreateShader(Shader::Stage::PS, PS_TRI_PROJ, L"PSTriProj.cso"), false);
	N_RETURN(m_shaderPool.CreateShader(Shader::Stage::PS, PS_TRI_PROJ_SOLID, L"PSTriProjSolid.cso"), false);
	N_RETURN(m_shaderPool.CreateShader(Shader::Stage::PS, PS_TRI_PROJ_UNION, L"PSTriProjUnion.cso"), false);
	N_RETURN(m_shaderPool.CreateShader(Shader::Stage::PS, PS_TRI_PROJ_UNION_SOLID, L"PSTriProjUnionSolid.cso"), false);
	N_RETURN(m_shaderPool.CreateShader(Shader::Stage::PS, PS_SIMPLE, L"PSSimple.cso"), false);
	N_RETURN(m_shaderPool.CreateShader(Shader::Stage::PS, PS_RAY_CAST, L"PSRayCast.cso"), false);

	N_RETURN(m_shaderPool.CreateShader(Shader::Stage::CS, CS_FILL_SOLID, L"CSFillSolid.cso"), false);

	return true;
}

bool Voxelizer::createVB(uint32_t numVert, uint32_t stride, const uint8_t *pData, Resource &vbUpload)
{
	N_RETURN(m_vertexBuffer.Create(m_device, numVert, stride, D3D12_RESOURCE_FLAG_NONE,
		D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COPY_DEST), false);

	return m_vertexBuffer.Upload(m_commandList, vbUpload, pData, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}

bool Voxelizer::createIB(uint32_t numIndices, const uint32_t *pData, Resource &ibUpload)
{
	m_numIndices = numIndices;
	N_RETURN(m_indexbuffer.Create(m_device, sizeof(uint32_t) * numIndices, DXGI_FORMAT_R32_UINT, D3D12_RESOURCE_FLAG_NONE,
		D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COPY_DEST), false);

	return m_indexbuffer.Upload(m_commandList, ibUpload, pData, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}

bool Voxelizer::createCBs()
{
	// Common CBs
	{
		N_RETURN(m_cbMatrices.Create(m_device, sizeof(CBMatrices) * FrameCount, FrameCount), false);
		N_RETURN(m_cbPerFrame.Create(m_device, sizeof(CBPerFrame) * FrameCount, FrameCount), false);
		N_RETURN(m_cbPerObject.Create(m_device, sizeof(CBPerObject) * FrameCount, FrameCount), false);
	}

	// Immutable CBs
	{
		N_RETURN(m_cbBound.Create(m_device, sizeof(XMFLOAT4)), false);

		const auto pCbBound = reinterpret_cast<XMFLOAT4*>(m_cbBound.Map());
		*pCbBound = m_bound;
		m_cbBound.Unmap();
	}

	m_cbPerMipLevels.resize(m_numLevels);
	for (auto i = 0u; i < m_numLevels; ++i)
	{
		auto &cb = m_cbPerMipLevels[i];
		const auto gridSize = static_cast<float>(GRID_SIZE >> i);
		N_RETURN(cb.Create(m_device, sizeof(XMFLOAT4)), false);

		const auto pCbData = reinterpret_cast<float*>(cb.Map());
		*pCbData = gridSize;
		cb.Unmap();
	}

	return true;
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

	m_inputLayout = m_graphicsPipelineCache.CreateInputLayout(inputElementDescs);
}

bool Voxelizer::prevoxelize(uint8_t mipLevel)
{
	// Get CBVs
	Util::DescriptorTable utilCbvTable;
	utilCbvTable.SetDescriptors(0, 1, &m_cbBound.GetCBV());
	X_RETURN(m_cbvTables[CBV_TABLE_VOXELIZE], utilCbvTable.GetCbvSrvUavTable(m_descriptorTableCache), false);

	Util::DescriptorTable utilCbvPerMipLevelTable;
	utilCbvPerMipLevelTable.SetDescriptors(0, 1, &m_cbPerMipLevels[mipLevel].GetCBV());
	X_RETURN(m_cbvTables[CBV_TABLE_PER_MIP], utilCbvPerMipLevelTable.GetCbvSrvUavTable(m_descriptorTableCache), false);

	// Get SRVs
	const Descriptor srvs[] = { m_indexbuffer.GetSRV(), m_vertexBuffer.GetSRV() };
	Util::DescriptorTable utilSrvTable;
	utilSrvTable.SetDescriptors(0, static_cast<uint32_t>(size(srvs)), srvs);
	X_RETURN(m_srvTables[SRV_TABLE_VB_IB], utilSrvTable.GetCbvSrvUavTable(m_descriptorTableCache), false);

	// Get SRVs and UAVs
	for (auto i = 0ui8; i < FrameCount; ++i)
	{
		// Get SRV
		Util::DescriptorTable utilSrvTable;
		utilSrvTable.SetDescriptors(0, 1, &m_KBufferDepths[i].GetSRV());
		X_RETURN(m_srvTables[SRV_K_DEPTH + i], utilSrvTable.GetCbvSrvUavTable(m_descriptorTableCache), false);

		Util::DescriptorTable utilUavGridTable;
		utilUavGridTable.SetDescriptors(0, 1, &m_grids[i].GetUAV());
		X_RETURN(m_uavTables[i][UAV_TABLE_VOXELIZE], utilUavGridTable.GetCbvSrvUavTable(m_descriptorTableCache), false);

		Util::DescriptorTable utilUavKBufferTable;
		utilUavKBufferTable.SetDescriptors(0, 1, &m_KBufferDepths[i].GetUAV());
		X_RETURN(m_uavTables[i][UAV_TABLE_KBUFFER], utilUavKBufferTable.GetCbvSrvUavTable(m_descriptorTableCache), false);
	}

	// Get graphics pipeline layouts
	{
		Util::PipelineLayout utilPipelineLayout;
		utilPipelineLayout.SetRange(0, DescriptorType::CBV, 2, 0);
		utilPipelineLayout.SetRange(1, DescriptorType::CBV, 1, 0);
		utilPipelineLayout.SetRange(2, DescriptorType::UAV, 2, 0);
		utilPipelineLayout.SetRange(3, DescriptorType::SRV, static_cast<uint32_t>(size(srvs)), 0);
		utilPipelineLayout.SetShaderStage(0, Shader::Stage::VS);
		utilPipelineLayout.SetShaderStage(1, Shader::Stage::PS);
		utilPipelineLayout.SetShaderStage(2, Shader::Stage::PS);
		utilPipelineLayout.SetShaderStage(3, Shader::Stage::VS);
		X_RETURN(m_pipelineLayouts[PASS_VOXELIZE], utilPipelineLayout.GetPipelineLayout(
			m_pipelineLayoutCache, D3D12_ROOT_SIGNATURE_FLAG_NONE, L"VoxelizationPass"), false);
	}

	{
		Util::PipelineLayout utilPipelineLayout;
		utilPipelineLayout.SetRange(0, DescriptorType::CBV, 1, 0);
		utilPipelineLayout.SetRange(1, DescriptorType::CBV, 1, 0);
		utilPipelineLayout.SetRange(2, DescriptorType::UAV, 2, 0);
		utilPipelineLayout.SetShaderStage(0, Shader::Stage::VS);
		utilPipelineLayout.SetShaderStage(1, Shader::Stage::PS);
		utilPipelineLayout.SetShaderStage(2, Shader::Stage::PS);
		X_RETURN(m_pipelineLayouts[PASS_VOXELIZE_UNION], utilPipelineLayout.GetPipelineLayout(
			m_pipelineLayoutCache, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT,
			L"VoxelizationByUnionPass"), false);
	}

	{
		Util::PipelineLayout utilPipelineLayout;
		utilPipelineLayout.SetRange(0, DescriptorType::CBV, 1, 0);
		utilPipelineLayout.SetRange(1, DescriptorType::CBV, 1, 0);
		utilPipelineLayout.SetRange(2, DescriptorType::UAV, 2, 0);
		utilPipelineLayout.SetRange(3, DescriptorType::CBV, 1, 0);
		utilPipelineLayout.SetShaderStage(0, Shader::Stage::VS);
		utilPipelineLayout.SetShaderStage(1, Shader::Stage::PS);
		utilPipelineLayout.SetShaderStage(2, Shader::Stage::PS);
		utilPipelineLayout.SetShaderStage(3, Shader::Stage::DS);
		X_RETURN(m_pipelineLayouts[PASS_VOXELIZE_TESS], utilPipelineLayout.GetPipelineLayout(
			m_pipelineLayoutCache, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT,
			L"VoxelizationByTessellationPass"), false);
	}

	// Get graphics pipelines
	{
		Graphics::State state;
		state.SetPipelineLayout(m_pipelineLayouts[PASS_VOXELIZE]);
		state.SetShader(Shader::Stage::VS, m_shaderPool.GetShader(Shader::Stage::VS, VS_TRI_PROJ));
		state.SetShader(Shader::Stage::PS, m_shaderPool.GetShader(Shader::Stage::PS, PS_TRI_PROJ));
		state.DSSetState(Graphics::DepthStencilPreset::DEPTH_STENCIL_NONE, m_graphicsPipelineCache);
		state.IASetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		state.RSSetState(Graphics::RasterizerPreset::CULL_NONE, m_graphicsPipelineCache);
		state.OMSetNumRenderTargets(0);
		X_RETURN(m_pipelines[PASS_VOXELIZE], state.GetPipeline(m_graphicsPipelineCache, L"Voxelization"), false);

		state.SetShader(Shader::Stage::PS, m_shaderPool.GetShader(Shader::Stage::PS, PS_TRI_PROJ_SOLID));
		X_RETURN(m_pipelines[PASS_VOXELIZE_SOLID], state.GetPipeline(m_graphicsPipelineCache, L"VoxelizationSolid"), false);

		state.IASetInputLayout(m_inputLayout);
		state.SetPipelineLayout(m_pipelineLayouts[PASS_VOXELIZE_UNION]);
		state.SetShader(Shader::Stage::VS, m_shaderPool.GetShader(Shader::Stage::VS, VS_TRI_PROJ_UNION));
		state.SetShader(Shader::Stage::PS, m_shaderPool.GetShader(Shader::Stage::PS, PS_TRI_PROJ_UNION));
		X_RETURN(m_pipelines[PASS_VOXELIZE_UNION], state.GetPipeline(m_graphicsPipelineCache, L"VoxelizationUnion"), false);

		state.SetShader(Shader::Stage::PS, m_shaderPool.GetShader(Shader::Stage::PS, PS_TRI_PROJ_UNION_SOLID));
		X_RETURN(m_pipelines[PASS_VOXELIZE_UNION_SOLID], state.GetPipeline(m_graphicsPipelineCache, L"VoxelizationUnionSolid"), false);

		state.SetPipelineLayout(m_pipelineLayouts[PASS_VOXELIZE_TESS]);
		state.SetShader(Shader::Stage::VS, m_shaderPool.GetShader(Shader::Stage::VS, VS_TRI_PROJ_TESS));
		state.SetShader(Shader::Stage::HS, m_shaderPool.GetShader(Shader::Stage::HS, HS_TRI_PROJ));
		state.SetShader(Shader::Stage::DS, m_shaderPool.GetShader(Shader::Stage::DS, DS_TRI_PROJ));
		state.SetShader(Shader::Stage::PS, m_shaderPool.GetShader(Shader::Stage::PS, PS_TRI_PROJ));
		state.IASetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH);
		X_RETURN(m_pipelines[PASS_VOXELIZE_TESS], state.GetPipeline(m_graphicsPipelineCache, L"VoxelizationTess"), false);

		state.SetShader(Shader::Stage::PS, m_shaderPool.GetShader(Shader::Stage::PS, PS_TRI_PROJ_SOLID));
		X_RETURN(m_pipelines[PASS_VOXELIZE_TESS_SOLID], state.GetPipeline(m_graphicsPipelineCache, L"VoxelizatioTessSolid"), false);
	}

	// Get compute pipeline layout
	{
		Util::PipelineLayout utilPipelineLayout;
		utilPipelineLayout.SetRange(0, DescriptorType::CBV, 1, 0);
		utilPipelineLayout.SetRange(1, DescriptorType::SRV, 1, 0);
		utilPipelineLayout.SetRange(2, DescriptorType::UAV, 1, 0);
		X_RETURN(m_pipelineLayouts[PASS_FILL_SOLID], utilPipelineLayout.GetPipelineLayout(
			m_pipelineLayoutCache, D3D12_ROOT_SIGNATURE_FLAG_NONE, L"SolidFillPass"), false);
	}

	// Get compute pipeline
	{
		Compute::State state;
		state.SetPipelineLayout(m_pipelineLayouts[PASS_FILL_SOLID]);
		state.SetShader(m_shaderPool.GetShader(Shader::Stage::CS, CS_FILL_SOLID));
		X_RETURN(m_pipelines[PASS_FILL_SOLID], state.GetPipeline(m_computePipelineCache, L"SolidFill"), false);
	}

	return true;
}

bool Voxelizer::prerenderBoxArray(Format rtFormat, Format dsFormat)
{
	for (auto i = 0ui8; i < FrameCount; ++i)
	{
		// Get CBV
		Util::DescriptorTable utilCbvTable;
		utilCbvTable.SetDescriptors(0, 1, &m_cbMatrices.GetCBV(i));
		X_RETURN(m_cbvTables[CBV_TABLE_MATRICES + i], utilCbvTable.GetCbvSrvUavTable(m_descriptorTableCache), false);

		// Get SRV
		if (!m_srvTables[SRV_TABLE_GRID + i])
		{
			Util::DescriptorTable utilSrvTable;
			utilSrvTable.SetDescriptors(0, 1, &m_grids[i].GetSRV());
			X_RETURN(m_srvTables[SRV_TABLE_GRID + i], utilSrvTable.GetCbvSrvUavTable(m_descriptorTableCache), false);
		}
	}

	// Get pipeline layout
	Util::PipelineLayout utilPipelineLayout;
	utilPipelineLayout.SetRange(0, DescriptorType::CBV, 1, 0);
	utilPipelineLayout.SetRange(1, DescriptorType::SRV, 1, 0);
	utilPipelineLayout.SetShaderStage(0, Shader::Stage::VS);
	utilPipelineLayout.SetShaderStage(1, Shader::Stage::VS);
	X_RETURN(m_pipelineLayouts[PASS_DRAW_AS_BOX], utilPipelineLayout.GetPipelineLayout(
		m_pipelineLayoutCache, D3D12_ROOT_SIGNATURE_FLAG_NONE, L"DrawAsBoxPass"), false);

	// Get pipeline
	Graphics::State state;
	state.SetPipelineLayout(m_pipelineLayouts[PASS_DRAW_AS_BOX]);
	state.SetShader(Shader::Stage::VS, m_shaderPool.GetShader(Shader::Stage::VS, VS_BOX_ARRAY));
	state.SetShader(Shader::Stage::PS, m_shaderPool.GetShader(Shader::Stage::PS, PS_SIMPLE));
	state.IASetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	state.OMSetRTVFormats(&rtFormat, 1);
	state.OMSetDSVFormat(dsFormat);
	X_RETURN(m_pipelines[PASS_DRAW_AS_BOX], state.GetPipeline(m_graphicsPipelineCache, L"DrawAsBox"), false);

	return true;
}

bool Voxelizer::prerayCast(Format rtFormat, Format dsFormat)
{
	for (auto i = 0ui8; i < FrameCount; ++i)
	{
		// Get CBV
		Util::DescriptorTable utilCbvTable;
		utilCbvTable.SetDescriptors(0, 1, &m_cbPerObject.GetCBV(i));
		X_RETURN(m_cbvTables[CBV_TABLE_PER_OBJ + i], utilCbvTable.GetCbvSrvUavTable(m_descriptorTableCache), false);

		// Get SRV
		if (!m_srvTables[SRV_TABLE_GRID + i])
		{
			Util::DescriptorTable utilSrvTable;
			utilSrvTable.SetDescriptors(0, 1, &m_grids[i].GetSRV());
			X_RETURN(m_srvTables[SRV_TABLE_GRID + i], utilSrvTable.GetCbvSrvUavTable(m_descriptorTableCache), false);
		}
	}

	// Create the sampler table
	Util::DescriptorTable samplerTable;
	const auto sampler = LINEAR_CLAMP;
	samplerTable.SetSamplers(0, 1, &sampler, m_descriptorTableCache);
	X_RETURN(m_samplerTable, samplerTable.GetSamplerTable(m_descriptorTableCache), false);

	// Get pipeline layout
	Util::PipelineLayout utilPipelineLayout;
	utilPipelineLayout.SetRange(0, DescriptorType::CBV, 1, 0);
	utilPipelineLayout.SetRange(1, DescriptorType::SRV, 1, 0);
	utilPipelineLayout.SetRange(2, DescriptorType::SAMPLER, 1, 0);
	utilPipelineLayout.SetShaderStage(0, Shader::Stage::PS);
	utilPipelineLayout.SetShaderStage(1, Shader::Stage::PS);
	utilPipelineLayout.SetShaderStage(2, Shader::Stage::PS);
	X_RETURN(m_pipelineLayouts[PASS_RAY_CAST], utilPipelineLayout.GetPipelineLayout(
		m_pipelineLayoutCache, D3D12_ROOT_SIGNATURE_FLAG_NONE, L"RayCastPass"), false);

	// Get pipeline
	Graphics::State state;
	state.SetPipelineLayout(m_pipelineLayouts[PASS_RAY_CAST]);
	state.SetShader(Shader::Stage::VS, m_shaderPool.GetShader(Shader::Stage::VS, VS_SCREEN_QUAD));
	state.SetShader(Shader::Stage::PS, m_shaderPool.GetShader(Shader::Stage::PS, PS_RAY_CAST));
	state.DSSetState(Graphics::DepthStencilPreset::DEPTH_STENCIL_NONE, m_graphicsPipelineCache);
	state.IASetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	state.OMSetBlendState(Graphics::BlendPreset::NON_PRE_MUL, m_graphicsPipelineCache);
	state.OMSetRTVFormats(&rtFormat, 1);
	X_RETURN(m_pipelines[PASS_RAY_CAST], state.GetPipeline(m_graphicsPipelineCache, L"RayCast"), false);

	return true;
}

void Voxelizer::voxelize(Method voxMethod, uint32_t frameIndex, bool depthPeel, uint8_t mipLevel)
{
	auto layoutIdx = PASS_VOXELIZE;
	auto pipeIdx = depthPeel ? PASS_VOXELIZE_SOLID : PASS_VOXELIZE;
	auto instanceCount = m_numIndices / 3;

	switch (voxMethod)
	{
	case TRI_PROJ_TESS:
		layoutIdx = PASS_VOXELIZE_TESS;
		pipeIdx = depthPeel ? PASS_VOXELIZE_TESS_SOLID : PASS_VOXELIZE_TESS;
		instanceCount = 1;
		break;
	case TRI_PROJ_UNION:
		layoutIdx = PASS_VOXELIZE_UNION;
		pipeIdx = depthPeel ? PASS_VOXELIZE_UNION_SOLID : PASS_VOXELIZE_UNION;
		instanceCount = 3;
		break;
	}

	// Set descriptor tables
	m_commandList.SetGraphicsPipelineLayout(m_pipelineLayouts[layoutIdx]);
	m_grids[frameIndex].Barrier(m_commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	if (depthPeel) m_KBufferDepths[frameIndex].Barrier(m_commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	m_commandList.SetGraphicsDescriptorTable(0, m_cbvTables[CBV_TABLE_VOXELIZE]);
	m_commandList.SetGraphicsDescriptorTable(1, m_cbvTables[CBV_TABLE_PER_MIP]);
	m_commandList.SetGraphicsDescriptorTable(2, m_uavTables[frameIndex][UAV_TABLE_VOXELIZE]);
	switch (voxMethod)
	{
	case TRI_PROJ:
		m_commandList.SetGraphicsDescriptorTable(3, m_srvTables[SRV_TABLE_VB_IB]);
		break;
	case TRI_PROJ_TESS:
		m_commandList.SetGraphicsDescriptorTable(3, m_cbvTables[CBV_TABLE_PER_MIP]);
		break;
	}

	// Set pipeline state
	m_commandList.SetPipelineState(m_pipelines[pipeIdx]);

	// Set viewport
	const auto gridSize = GRID_SIZE >> mipLevel;
	const auto fGridSize = static_cast<float>(gridSize);
	Viewport viewport(0.0f, 0.0f, fGridSize, fGridSize);
	RectRange scissorRect(0, 0, gridSize, gridSize);
	m_commandList.RSSetViewports(1, &viewport);
	m_commandList.RSSetScissorRects(1, &scissorRect);

	// Record commands.
	m_commandList.ClearUnorderedAccessViewUint(*m_uavTables[frameIndex][UAV_TABLE_VOXELIZE], m_grids[frameIndex].GetUAV(),
		m_grids[frameIndex].GetResource(), XMVECTORU32{ 0 }.u);
	if (depthPeel) m_commandList.ClearUnorderedAccessViewUint(*m_uavTables[frameIndex][UAV_TABLE_KBUFFER], m_KBufferDepths[frameIndex].GetUAV(),
		m_KBufferDepths[frameIndex].GetResource(), XMVECTORU32{ UINT32_MAX }.u);

	// Set IA
	if (voxMethod != TRI_PROJ)
	{
		m_commandList.IASetVertexBuffers(0, 1, &m_vertexBuffer.GetVBV());
		m_commandList.IASetIndexBuffer(m_indexbuffer.GetIBV());
	}

	m_commandList.IASetPrimitiveTopology(voxMethod == TRI_PROJ_TESS ?
		D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST : D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	if (voxMethod == TRI_PROJ)
		m_commandList.Draw(3, instanceCount, 0, 0);
	else
		m_commandList.DrawIndexed(m_numIndices, instanceCount, 0, 0, 0);
}

void Voxelizer::voxelizeSolid(Method voxMethod, uint32_t frameIndex, uint8_t mipLevel)
{
	// Surface voxelization with depth peeling
	voxelize(voxMethod, frameIndex, true, mipLevel);

	// Set descriptor tables
	m_commandList.SetComputePipelineLayout(m_pipelineLayouts[PASS_FILL_SOLID]);
	m_grids[frameIndex].Barrier(m_commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	m_KBufferDepths[frameIndex].Barrier(m_commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	m_commandList.SetComputeDescriptorTable(0, m_cbvTables[CBV_TABLE_PER_MIP]);
	m_commandList.SetComputeDescriptorTable(1, m_srvTables[SRV_K_DEPTH + frameIndex]);
	m_commandList.SetComputeDescriptorTable(2, m_uavTables[frameIndex][UAV_TABLE_VOXELIZE]);

	// Set pipeline state
	m_commandList.SetPipelineState(m_pipelines[PASS_FILL_SOLID]);

	// Record commands.
	m_commandList.Dispatch(GRID_SIZE / 32, GRID_SIZE / 16, GRID_SIZE);
}

void Voxelizer::renderBoxArray(uint32_t frameIndex, const RenderTargetTable &rtvs, const Descriptor &dsv)
{
	// Set descriptor tables
	m_commandList.SetGraphicsPipelineLayout(m_pipelineLayouts[PASS_DRAW_AS_BOX]);
	m_grids[frameIndex].Barrier(m_commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	m_commandList.SetGraphicsDescriptorTable(0, m_cbvTables[CBV_TABLE_MATRICES + frameIndex]);
	m_commandList.SetGraphicsDescriptorTable(1, m_srvTables[SRV_TABLE_GRID + frameIndex]);

	// Set pipeline state
	m_commandList.SetPipelineState(m_pipelines[PASS_DRAW_AS_BOX]);

	// Set viewport
	const auto gridSize = GRID_SIZE >> SHOW_MIP;
	Viewport viewport(0.0f, 0.0f, m_viewport.x, m_viewport.y);
	RectRange scissorRect(0, 0, static_cast<long>(m_viewport.x), static_cast<long>(m_viewport.y));
	m_commandList.RSSetViewports(1, &viewport);
	m_commandList.RSSetScissorRects(1, &scissorRect);

	m_commandList.OMSetRenderTargets(1, rtvs, &dsv);

	// Record commands.
	m_commandList.IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	m_commandList.Draw(4, 6 * gridSize * gridSize * gridSize, 0, 0);
}

void Voxelizer::renderRayCast(uint32_t frameIndex, const RenderTargetTable &rtvs, const Descriptor &dsv)
{
	// Set descriptor tables
	m_commandList.SetGraphicsPipelineLayout(m_pipelineLayouts[PASS_RAY_CAST]);
	m_grids[frameIndex].Barrier(m_commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	m_commandList.SetGraphicsDescriptorTable(0, m_cbvTables[CBV_TABLE_PER_OBJ + frameIndex]);
	m_commandList.SetGraphicsDescriptorTable(1, m_srvTables[SRV_TABLE_GRID + frameIndex]);
	m_commandList.SetGraphicsDescriptorTable(2, m_samplerTable);

	// Set pipeline state
	m_commandList.SetPipelineState(m_pipelines[PASS_RAY_CAST]);

	// Set viewport
	Viewport viewport(0.0f, 0.0f, m_viewport.x, m_viewport.y);
	RectRange scissorRect(0, 0, static_cast<long>(m_viewport.x), static_cast<long>(m_viewport.y));
	m_commandList.RSSetViewports(1, &viewport);
	m_commandList.RSSetScissorRects(1, &scissorRect);

	m_commandList.OMSetRenderTargets(1, rtvs, nullptr);

	// Record commands.
	m_commandList.IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	m_commandList.Draw(3, 1, 0, 0);
}
