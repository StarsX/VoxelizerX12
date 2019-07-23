//--------------------------------------------------------------------------------------
// By XU, Tianchen
//--------------------------------------------------------------------------------------

#include "ObjLoader.h"
#include "Voxelizer.h"

using namespace std;
using namespace DirectX;
using namespace XUSG;

Voxelizer::Voxelizer(const Device& device) :
	m_device(device)
{
	m_graphicsPipelineCache.SetDevice(device);
	m_computePipelineCache.SetDevice(device);
	m_descriptorTableCache.SetDevice(device);
	m_pipelineLayoutCache.SetDevice(device);
}

Voxelizer::~Voxelizer()
{
}

bool Voxelizer::Init(const CommandList& commandList, uint32_t width, uint32_t height,
	Format rtFormat, Format dsFormat, vector<Resource>& uploaders, const char* fileName)
{
	m_viewport.x = static_cast<float>(width);
	m_viewport.y = static_cast<float>(height);

	// Create shaders
	N_RETURN(createShaders(), false);

	// Load inputs
	ObjLoader objLoader;
	if (!objLoader.Import(fileName, true, true)) return false;

	createInputLayout();
	N_RETURN(createVB(commandList, objLoader.GetNumVertices(), objLoader.GetVertexStride(), objLoader.GetVertices(), uploaders), false);
	N_RETURN(createIB(commandList, objLoader.GetNumIndices(), objLoader.GetIndices(), uploaders), false);

	// Extract boundary
	const auto center = objLoader.GetCenter();
	m_bound = XMFLOAT4(center.x, center.y, center.z, objLoader.GetRadius());

	m_numLevels = max(static_cast<uint32_t>(log2(GRID_SIZE)), 1);
	N_RETURN(createCBs(commandList, uploaders), false);

	for (auto& grid : m_grids)
		N_RETURN(grid.Create(m_device, GRID_SIZE, GRID_SIZE, GRID_SIZE, DXGI_FORMAT_R10G10B10A2_UNORM, BIND_PACKED_UAV), false);

	for (auto& KBufferDepth : m_KBufferDepths)
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
	pCbPerObject->localSpaceLightPt = XMVector3TransformCoord(XMVectorSet(-10.0f, 45.0f, -75.0f, 0.0f), worldI);
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

void Voxelizer::Render(const CommandList& commandList, bool solid, Method voxMethod,
	uint32_t frameIndex, const RenderTargetTable& rtvs, const Descriptor& dsv)
{
	if (solid)
	{
		const DescriptorPool descriptorPools[] =
		{
			m_descriptorTableCache.GetDescriptorPool(CBV_SRV_UAV_POOL),
			m_descriptorTableCache.GetDescriptorPool(SAMPLER_POOL)
		};
		commandList.SetDescriptorPools(static_cast<uint32_t>(size(descriptorPools)), descriptorPools);

		voxelizeSolid(commandList, voxMethod, frameIndex);
		renderRayCast(commandList, frameIndex, rtvs, dsv);
	}
	else
	{
		const DescriptorPool descriptorPools[] =
		{ m_descriptorTableCache.GetDescriptorPool(CBV_SRV_UAV_POOL) };
		commandList.SetDescriptorPools(static_cast<uint32_t>(size(descriptorPools)), descriptorPools);

		voxelize(commandList, voxMethod, frameIndex);
		renderBoxArray(commandList, frameIndex, rtvs, dsv);
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

bool Voxelizer::createVB(const CommandList& commandList, uint32_t numVert, uint32_t stride,
	const uint8_t* pData, vector<Resource>& uploaders)
{
	N_RETURN(m_vertexBuffer.Create(m_device, numVert, stride, D3D12_RESOURCE_FLAG_NONE,
		D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COPY_DEST), false);
	uploaders.push_back(nullptr);

	return m_vertexBuffer.Upload(commandList, uploaders.back(), pData, stride * numVert,
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}

bool Voxelizer::createIB(const CommandList& commandList, uint32_t numIndices,
	const uint32_t* pData, vector<Resource>& uploaders)
{
	m_numIndices = numIndices;
	const uint32_t byteWidth = sizeof(uint32_t) * numIndices;
	N_RETURN(m_indexbuffer.Create(m_device, byteWidth, DXGI_FORMAT_R32_UINT, D3D12_RESOURCE_FLAG_NONE,
		D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COPY_DEST), false);
	uploaders.push_back(nullptr);

	return m_indexbuffer.Upload(commandList, uploaders.back(), pData,
		byteWidth, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}

bool Voxelizer::createCBs(const CommandList& commandList, vector<Resource>& uploaders)
{
	// Common CBs
	{
		N_RETURN(m_cbMatrices.Create(m_device, sizeof(CBMatrices) * FrameCount, FrameCount), false);
		N_RETURN(m_cbPerFrame.Create(m_device, sizeof(CBPerFrame) * FrameCount, FrameCount), false);
		N_RETURN(m_cbPerObject.Create(m_device, sizeof(CBPerObject) * FrameCount, FrameCount), false);
	}

	// Immutable CBs
	{
		N_RETURN(m_cbBound.Create(m_device, sizeof(XMFLOAT4), 1, nullptr, D3D12_HEAP_TYPE_DEFAULT), false);
		uploaders.push_back(nullptr);
		m_cbBound.Upload(commandList, uploaders.back(), &m_bound, sizeof(XMFLOAT4));
	}

	m_cbPerMipLevels.resize(m_numLevels);
	for (auto i = 0u; i < m_numLevels; ++i)
	{
		auto& cb = m_cbPerMipLevels[i];
		const auto gridSize = static_cast<float>(GRID_SIZE >> i);
		N_RETURN(cb.Create(m_device, sizeof(XMFLOAT4), 1, nullptr, D3D12_HEAP_TYPE_DEFAULT), false);

		uploaders.push_back(nullptr);
		cb.Upload(commandList, uploaders.back(), &gridSize, sizeof(float));
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
		utilPipelineLayout.SetRange(0, DescriptorType::CBV, 2, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
		utilPipelineLayout.SetRange(1, DescriptorType::CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
		utilPipelineLayout.SetRange(2, DescriptorType::UAV, 2, 0, 0,
			D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE);
		utilPipelineLayout.SetRange(3, DescriptorType::SRV, static_cast<uint32_t>(size(srvs)), 0,
			0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
		utilPipelineLayout.SetShaderStage(0, Shader::Stage::VS);
		utilPipelineLayout.SetShaderStage(1, Shader::Stage::PS);
		utilPipelineLayout.SetShaderStage(2, Shader::Stage::PS);
		utilPipelineLayout.SetShaderStage(3, Shader::Stage::VS);
		X_RETURN(m_pipelineLayouts[PASS_VOXELIZE], utilPipelineLayout.GetPipelineLayout(
			m_pipelineLayoutCache, D3D12_ROOT_SIGNATURE_FLAG_NONE, L"VoxelizationPass"), false);
	}

	{
		Util::PipelineLayout utilPipelineLayout;
		utilPipelineLayout.SetRange(0, DescriptorType::CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
		utilPipelineLayout.SetRange(1, DescriptorType::CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
		utilPipelineLayout.SetRange(2, DescriptorType::UAV, 2, 0, 0,
			D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE);
		utilPipelineLayout.SetShaderStage(0, Shader::Stage::VS);
		utilPipelineLayout.SetShaderStage(1, Shader::Stage::PS);
		utilPipelineLayout.SetShaderStage(2, Shader::Stage::PS);
		X_RETURN(m_pipelineLayouts[PASS_VOXELIZE_UNION], utilPipelineLayout.GetPipelineLayout(
			m_pipelineLayoutCache, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT,
			L"VoxelizationByUnionPass"), false);
	}

	{
		Util::PipelineLayout utilPipelineLayout;
		utilPipelineLayout.SetRange(0, DescriptorType::CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
		utilPipelineLayout.SetRange(1, DescriptorType::CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
		utilPipelineLayout.SetRange(2, DescriptorType::UAV, 2, 0, 0,
			D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE);
		utilPipelineLayout.SetRange(3, DescriptorType::CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
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
	utilPipelineLayout.SetRange(0, DescriptorType::CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
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
	utilPipelineLayout.SetRange(0, DescriptorType::CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
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
	state.OMSetRTVFormats(&rtFormat, 1);
	X_RETURN(m_pipelines[PASS_RAY_CAST], state.GetPipeline(m_graphicsPipelineCache, L"RayCast"), false);

	return true;
}

void Voxelizer::voxelize(const CommandList& commandList, Method voxMethod, uint32_t frameIndex, bool depthPeel, uint8_t mipLevel)
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

	// Set resource barriers
	ResourceBarrier barriers[2];
	auto numBarriers = m_grids[frameIndex].SetBarrier(barriers, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	if (depthPeel) numBarriers = m_KBufferDepths[frameIndex].SetBarrier(barriers, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, numBarriers);
	commandList.Barrier(numBarriers, barriers);

	// Set descriptor tables
	commandList.SetGraphicsPipelineLayout(m_pipelineLayouts[layoutIdx]);
	commandList.SetGraphicsDescriptorTable(0, m_cbvTables[CBV_TABLE_VOXELIZE]);
	commandList.SetGraphicsDescriptorTable(1, m_cbvTables[CBV_TABLE_PER_MIP]);
	commandList.SetGraphicsDescriptorTable(2, m_uavTables[frameIndex][UAV_TABLE_VOXELIZE]);
	switch (voxMethod)
	{
	case TRI_PROJ:
		commandList.SetGraphicsDescriptorTable(3, m_srvTables[SRV_TABLE_VB_IB]);
		break;
	case TRI_PROJ_TESS:
		commandList.SetGraphicsDescriptorTable(3, m_cbvTables[CBV_TABLE_PER_MIP]);
		break;
	}

	// Set pipeline state
	commandList.SetPipelineState(m_pipelines[pipeIdx]);

	// Set viewport
	const auto gridSize = GRID_SIZE >> mipLevel;
	const auto fGridSize = static_cast<float>(gridSize);
	Viewport viewport(0.0f, 0.0f, fGridSize, fGridSize);
	RectRange scissorRect(0, 0, gridSize, gridSize);
	commandList.RSSetViewports(1, &viewport);
	commandList.RSSetScissorRects(1, &scissorRect);

	// Record commands.
	commandList.ClearUnorderedAccessViewUint(*m_uavTables[frameIndex][UAV_TABLE_VOXELIZE], m_grids[frameIndex].GetUAV(),
		m_grids[frameIndex].GetResource(), XMVECTORU32{ 0 }.u);
	if (depthPeel) commandList.ClearUnorderedAccessViewUint(*m_uavTables[frameIndex][UAV_TABLE_KBUFFER], m_KBufferDepths[frameIndex].GetUAV(),
		m_KBufferDepths[frameIndex].GetResource(), XMVECTORU32{ UINT32_MAX }.u);

	// Set IA
	if (voxMethod != TRI_PROJ)
	{
		commandList.IASetVertexBuffers(0, 1, &m_vertexBuffer.GetVBV());
		commandList.IASetIndexBuffer(m_indexbuffer.GetIBV());
	}

	commandList.IASetPrimitiveTopology(voxMethod == TRI_PROJ_TESS ?
		D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST : D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	if (voxMethod == TRI_PROJ)
		commandList.Draw(3, instanceCount, 0, 0);
	else
		commandList.DrawIndexed(m_numIndices, instanceCount, 0, 0, 0);
}

void Voxelizer::voxelizeSolid(const CommandList& commandList, Method voxMethod, uint32_t frameIndex, uint8_t mipLevel)
{
	// Surface voxelization with depth peeling
	voxelize(commandList, voxMethod, frameIndex, true, mipLevel);

	// Set resource barriers
	ResourceBarrier barriers[2];
	auto numBarriers = m_grids[frameIndex].SetBarrier(barriers, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	numBarriers = m_KBufferDepths[frameIndex].SetBarrier(barriers, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	commandList.Barrier(numBarriers, barriers);

	// Set descriptor tables
	commandList.SetComputePipelineLayout(m_pipelineLayouts[PASS_FILL_SOLID]);

	commandList.SetComputeDescriptorTable(0, m_cbvTables[CBV_TABLE_PER_MIP]);
	commandList.SetComputeDescriptorTable(1, m_srvTables[SRV_K_DEPTH + frameIndex]);
	commandList.SetComputeDescriptorTable(2, m_uavTables[frameIndex][UAV_TABLE_VOXELIZE]);

	// Set pipeline state
	commandList.SetPipelineState(m_pipelines[PASS_FILL_SOLID]);

	// Record commands.
	commandList.Dispatch(GRID_SIZE / 32, GRID_SIZE / 16, GRID_SIZE);
}

void Voxelizer::renderBoxArray(const CommandList& commandList, uint32_t frameIndex, const RenderTargetTable& rtvs, const Descriptor& dsv)
{
	// Set resource barrier
	ResourceBarrier barrier;
	const auto numBarriers = m_grids[frameIndex].SetBarrier(&barrier, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	commandList.Barrier(numBarriers, &barrier);

	// Set descriptor tables
	commandList.SetGraphicsPipelineLayout(m_pipelineLayouts[PASS_DRAW_AS_BOX]);

	commandList.SetGraphicsDescriptorTable(0, m_cbvTables[CBV_TABLE_MATRICES + frameIndex]);
	commandList.SetGraphicsDescriptorTable(1, m_srvTables[SRV_TABLE_GRID + frameIndex]);

	// Set pipeline state
	commandList.SetPipelineState(m_pipelines[PASS_DRAW_AS_BOX]);

	// Set viewport
	const auto gridSize = GRID_SIZE >> SHOW_MIP;
	Viewport viewport(0.0f, 0.0f, m_viewport.x, m_viewport.y);
	RectRange scissorRect(0, 0, static_cast<long>(m_viewport.x), static_cast<long>(m_viewport.y));
	commandList.RSSetViewports(1, &viewport);
	commandList.RSSetScissorRects(1, &scissorRect);

	commandList.OMSetRenderTargets(1, rtvs, &dsv);

	// Record commands.
	commandList.IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	commandList.Draw(4, 6 * gridSize * gridSize * gridSize, 0, 0);
}

void Voxelizer::renderRayCast(const CommandList& commandList, uint32_t frameIndex, const RenderTargetTable& rtvs, const Descriptor& dsv)
{
	// Set resource barriers
	ResourceBarrier barrier;
	const auto numBarriers = m_grids[frameIndex].SetBarrier(&barrier, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	commandList.Barrier(numBarriers, &barrier);

	// Set descriptor tables
	commandList.SetGraphicsPipelineLayout(m_pipelineLayouts[PASS_RAY_CAST]);

	commandList.SetGraphicsDescriptorTable(0, m_cbvTables[CBV_TABLE_PER_OBJ + frameIndex]);
	commandList.SetGraphicsDescriptorTable(1, m_srvTables[SRV_TABLE_GRID + frameIndex]);
	commandList.SetGraphicsDescriptorTable(2, m_samplerTable);

	// Set pipeline state
	commandList.SetPipelineState(m_pipelines[PASS_RAY_CAST]);

	// Set viewport
	Viewport viewport(0.0f, 0.0f, m_viewport.x, m_viewport.y);
	RectRange scissorRect(0, 0, static_cast<long>(m_viewport.x), static_cast<long>(m_viewport.y));
	commandList.RSSetViewports(1, &viewport);
	commandList.RSSetScissorRects(1, &scissorRect);

	commandList.OMSetRenderTargets(1, rtvs, nullptr);

	// Record commands.
	commandList.IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	commandList.Draw(3, 1, 0, 0);
}
