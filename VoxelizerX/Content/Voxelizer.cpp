//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

#include "Optional/XUSGObjLoader.h"
#include "Voxelizer.h"

using namespace std;
using namespace DirectX;
using namespace XUSG;

struct CBMatrices
{
	DirectX::XMMATRIX worldViewProj;
	DirectX::XMFLOAT3X4 world;
	DirectX::XMFLOAT3X4 worldIT;
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

Voxelizer::Voxelizer(const Device::sptr& device) :
	m_device(device)
{
	m_graphicsPipelineCache = Graphics::PipelineCache::MakeUnique(device.get());
	m_computePipelineCache = Compute::PipelineCache::MakeUnique(device.get());
	m_descriptorTableCache = DescriptorTableCache::MakeUnique(device.get());
	m_pipelineLayoutCache = PipelineLayoutCache::MakeUnique(device.get());
}

Voxelizer::~Voxelizer()
{
}

bool Voxelizer::Init(CommandList* pCommandList, uint32_t width, uint32_t height, Format rtFormat,
	Format dsFormat, vector<Resource::uptr>& uploaders, const char* fileName, const XMFLOAT4& posScale)
{
	m_viewport.x = static_cast<float>(width);
	m_viewport.y = static_cast<float>(height);
	m_posScale = posScale;

	// Create shaders
	N_RETURN(createShaders(), false);

	// Load inputs
	ObjLoader objLoader;
	if (!objLoader.Import(fileName, true, true)) return false;

	N_RETURN(createInputLayout(), false);
	N_RETURN(createVB(pCommandList, objLoader.GetNumVertices(), objLoader.GetVertexStride(), objLoader.GetVertices(), uploaders), false);
	N_RETURN(createIB(pCommandList, objLoader.GetNumIndices(), objLoader.GetIndices(), uploaders), false);

	// Extract boundary
	const auto center = objLoader.GetCenter();
	m_bound = XMFLOAT4(center.x, center.y, center.z, objLoader.GetRadius());

	m_numLevels = max(static_cast<uint32_t>(log2(GRID_SIZE)), 1);
	N_RETURN(createCBs(pCommandList, uploaders), false);

#if	USE_MUTEX
	for (auto& grid : m_grid)
	{
		grid = Texture3D::MakeUnique();
		N_RETURN(grid->Create(m_device.get(), GRID_SIZE, GRID_SIZE, GRID_SIZE,
			Format::R32_FLOAT, ResourceFlag::ALLOW_UNORDERED_ACCESS), false);
	}

	m_mutex = Texture3D::MakeUnique();
	N_RETURN(m_mutex->Create(m_device.get(), GRID_SIZE, GRID_SIZE, GRID_SIZE,
		Format::R32_UINT, ResourceFlag::ALLOW_UNORDERED_ACCESS), false);
#else
	m_grid = Texture3D::MakeUnique();
	N_RETURN(m_grid->Create(m_device.get(), GRID_SIZE, GRID_SIZE, GRID_SIZE,
		Format::R10G10B10A2_UNORM, ResourceFlag::NEED_PACKED_UAV), false);
#endif

	m_KBufferDepth = Texture2D::MakeUnique();
	N_RETURN(m_KBufferDepth->Create(m_device.get(), GRID_SIZE, GRID_SIZE, Format::R32_UINT, static_cast<uint32_t>(GRID_SIZE * DEPTH_SCALE),
		ResourceFlag::ALLOW_UNORDERED_ACCESS | ResourceFlag::ALLOW_SIMULTANEOUS_ACCESS), false);

	// Prepare for rendering
	N_RETURN(prevoxelize(), false);
	N_RETURN(prerenderBoxArray(rtFormat, dsFormat), false);
	N_RETURN(prerayCast(rtFormat, dsFormat), false);

	return true;
}

void Voxelizer::UpdateFrame(uint8_t frameIndex, CXMVECTOR eyePt, CXMMATRIX viewProj)
{
	// General matrices
	const auto scl = 1.0f / m_bound.w;
	const auto world = XMMatrixScaling(m_bound.w, m_bound.w, m_bound.w) *
		XMMatrixTranslation(m_bound.x, m_bound.y, m_bound.z) *
		XMMatrixScaling(m_posScale.w, m_posScale.w, m_posScale.w) *
		XMMatrixTranslation(m_posScale.x, m_posScale.y, m_posScale.z);
	const auto worldI = XMMatrixInverse(nullptr, world);
	const auto worldViewProj = world * viewProj;
	const auto pCbMatrices = reinterpret_cast<CBMatrices*>(m_cbMatrices->Map(frameIndex));
	pCbMatrices->worldViewProj = XMMatrixTranspose(worldViewProj);
	XMStoreFloat3x4(&pCbMatrices->world, world);
	XMStoreFloat3x4(&pCbMatrices->worldIT, XMMatrixIdentity());

	// Screen space matrices
	const auto pCbPerObject = reinterpret_cast<CBPerObject*>(m_cbPerObject->Map(frameIndex));
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
	const auto pCbPerFrame = reinterpret_cast<CBPerFrame*>(m_cbPerFrame->Map(frameIndex));
	XMStoreFloat4(&pCbPerFrame->eyePos, eyePt);
}

void Voxelizer::Render(const CommandList* pCommandList, bool solid, Method voxMethod,
	uint8_t frameIndex, const Descriptor& rtv, const Descriptor& dsv)
{
	if (solid)
	{
		const DescriptorPool descriptorPools[] =
		{
			m_descriptorTableCache->GetDescriptorPool(CBV_SRV_UAV_POOL),
			m_descriptorTableCache->GetDescriptorPool(SAMPLER_POOL)
		};
		pCommandList->SetDescriptorPools(static_cast<uint32_t>(size(descriptorPools)), descriptorPools);

		voxelizeSolid(pCommandList, voxMethod);
		renderRayCast(pCommandList, frameIndex, rtv, dsv);
	}
	else
	{
		const DescriptorPool descriptorPools[] =
		{ m_descriptorTableCache->GetDescriptorPool(CBV_SRV_UAV_POOL) };
		pCommandList->SetDescriptorPools(static_cast<uint32_t>(size(descriptorPools)), descriptorPools);

		voxelize(pCommandList, voxMethod, frameIndex);
		renderBoxArray(pCommandList, frameIndex, rtv, dsv);
	}
}

bool Voxelizer::createShaders()
{
	m_shaderPool = ShaderPool::MakeUnique();
	N_RETURN(m_shaderPool->CreateShader(Shader::Stage::VS, VS_TRI_PROJ, L"VSTriProj.cso"), false);
	N_RETURN(m_shaderPool->CreateShader(Shader::Stage::VS, VS_TRI_PROJ_TESS, L"VSTriProjTess.cso"), false);
	N_RETURN(m_shaderPool->CreateShader(Shader::Stage::VS, VS_TRI_PROJ_UNION, L"VSTriProjUnion.cso"), false);
	N_RETURN(m_shaderPool->CreateShader(Shader::Stage::VS, VS_BOX_ARRAY, L"VSBoxArray.cso"), false);
	N_RETURN(m_shaderPool->CreateShader(Shader::Stage::VS, VS_SCREEN_QUAD, L"VSScreenQuad.cso"), false);

	N_RETURN(m_shaderPool->CreateShader(Shader::Stage::HS, HS_TRI_PROJ, L"HSTriProj.cso"), false);
	N_RETURN(m_shaderPool->CreateShader(Shader::Stage::DS, DS_TRI_PROJ, L"DSTriProj.cso"), false);

	N_RETURN(m_shaderPool->CreateShader(Shader::Stage::PS, PS_TRI_PROJ, L"PSTriProj.cso"), false);
	N_RETURN(m_shaderPool->CreateShader(Shader::Stage::PS, PS_TRI_PROJ_SOLID, L"PSTriProjSolid.cso"), false);
	N_RETURN(m_shaderPool->CreateShader(Shader::Stage::PS, PS_TRI_PROJ_UNION, L"PSTriProjUnion.cso"), false);
	N_RETURN(m_shaderPool->CreateShader(Shader::Stage::PS, PS_TRI_PROJ_UNION_SOLID, L"PSTriProjUnionSolid.cso"), false);
	N_RETURN(m_shaderPool->CreateShader(Shader::Stage::PS, PS_SIMPLE, L"PSSimple.cso"), false);
	N_RETURN(m_shaderPool->CreateShader(Shader::Stage::PS, PS_RAY_CAST, L"PSRayCast.cso"), false);

	N_RETURN(m_shaderPool->CreateShader(Shader::Stage::CS, CS_FILL_SOLID, L"CSFillSolid.cso"), false);

	return true;
}

bool Voxelizer::createVB(CommandList* pCommandList, uint32_t numVert, uint32_t stride,
	const uint8_t* pData, vector<Resource::uptr>& uploaders)
{
	m_vertexBuffer = VertexBuffer::MakeUnique();
	N_RETURN(m_vertexBuffer->Create(m_device.get(), numVert, stride,
		ResourceFlag::NONE, MemoryType::DEFAULT), false);
	uploaders.emplace_back(Resource::MakeUnique());

	return m_vertexBuffer->Upload(pCommandList, uploaders.back().get(), pData, stride * numVert);
}

bool Voxelizer::createIB(CommandList* pCommandList, uint32_t numIndices,
	const uint32_t* pData, vector<Resource::uptr>& uploaders)
{
	m_numIndices = numIndices;
	const uint32_t byteWidth = sizeof(uint32_t) * numIndices;
	m_indexbuffer = IndexBuffer::MakeUnique();
	N_RETURN(m_indexbuffer->Create(m_device.get(), byteWidth, Format::R32_UINT,
		ResourceFlag::NONE, MemoryType::DEFAULT), false);
	uploaders.emplace_back(Resource::MakeUnique());

	return m_indexbuffer->Upload(pCommandList, uploaders.back().get(), pData, byteWidth);
}

bool Voxelizer::createCBs(CommandList* pCommandList, vector<Resource::uptr>& uploaders)
{
	// Common CBs
	{
		m_cbMatrices = ConstantBuffer::MakeUnique();
		m_cbPerFrame = ConstantBuffer::MakeUnique();
		m_cbPerObject = ConstantBuffer::MakeUnique();
		N_RETURN(m_cbMatrices->Create(m_device.get(), sizeof(CBMatrices[FrameCount]), FrameCount), false);
		N_RETURN(m_cbPerFrame->Create(m_device.get(), sizeof(CBPerFrame[FrameCount]), FrameCount), false);
		N_RETURN(m_cbPerObject->Create(m_device.get(), sizeof(CBPerObject[FrameCount]), FrameCount), false);
	}

	// Immutable CBs
	{
		m_cbBound = ConstantBuffer::MakeUnique();
		N_RETURN(m_cbBound->Create(m_device.get(), sizeof(XMFLOAT4), 1, nullptr, MemoryType::DEFAULT), false);

		uploaders.emplace_back(Resource::MakeUnique());
		m_cbBound->Upload(pCommandList, uploaders.back().get(), &m_bound, sizeof(XMFLOAT4));
	}

	m_cbPerMipLevels.resize(m_numLevels);
	for (auto i = 0u; i < m_numLevels; ++i)
	{
		auto& cb = m_cbPerMipLevels[i];
		cb = ConstantBuffer::MakeUnique();
		const auto gridSize = static_cast<float>(GRID_SIZE >> i);
		N_RETURN(cb->Create(m_device.get(), sizeof(XMFLOAT4), 1, nullptr, MemoryType::DEFAULT), false);

		uploaders.emplace_back(Resource::MakeUnique());
		cb->Upload(pCommandList, uploaders.back().get(), &gridSize, sizeof(float));
	}

	return true;
}

bool Voxelizer::createInputLayout()
{
	// Define the vertex input layout.
	const InputElement inputElements[] =
	{
		{ "POSITION",	0, Format::R32G32B32_FLOAT, 0, 0,								InputClassification::PER_VERTEX_DATA, 0 },
		{ "NORMAL",		0, Format::R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,	InputClassification::PER_VERTEX_DATA, 0 }
	};

	X_RETURN(m_pInputLayout, m_graphicsPipelineCache->CreateInputLayout(inputElements, static_cast<uint32_t>(size(inputElements))), false);

	return true;
}

bool Voxelizer::prevoxelize(uint8_t mipLevel)
{
	// Get CBVs
	const auto utilCbvTable = Util::DescriptorTable::MakeUnique();
	utilCbvTable->SetDescriptors(0, 1, &m_cbBound->GetCBV());
	X_RETURN(m_cbvTables[CBV_TABLE_VOXELIZE], utilCbvTable->GetCbvSrvUavTable(m_descriptorTableCache.get()), false);

	const auto utilCbvPerMipLevelTable = Util::DescriptorTable::MakeUnique();
	utilCbvPerMipLevelTable->SetDescriptors(0, 1, &m_cbPerMipLevels[mipLevel]->GetCBV());
	X_RETURN(m_cbvTables[CBV_TABLE_PER_MIP], utilCbvPerMipLevelTable->GetCbvSrvUavTable(m_descriptorTableCache.get()), false);

	// Get SRVs
	const Descriptor srvs[] = { m_indexbuffer->GetSRV(), m_vertexBuffer->GetSRV() };
	const auto descriptorTable = Util::DescriptorTable::MakeUnique();
	descriptorTable->SetDescriptors(0, static_cast<uint32_t>(size(srvs)), srvs);
	X_RETURN(m_srvTables[SRV_TABLE_VB_IB], descriptorTable->GetCbvSrvUavTable(m_descriptorTableCache.get()), false);

	// Get UAVs and SRVs
	// Get UAVs
#if	USE_MUTEX
	for (uint8_t i = 0; i < 4; ++i)
	{
		const auto descriptorTable = Util::DescriptorTable::MakeUnique();
		descriptorTable->SetDescriptors(0, 1, &m_grid[i]->GetUAV());
		X_RETURN(m_uavTables[UAV_TABLE_VOXELIZE + i], descriptorTable->GetCbvSrvUavTable(m_descriptorTableCache.get()), false);
	}

	{
		const auto descriptorTable = Util::DescriptorTable::MakeUnique();
		descriptorTable->SetDescriptors(0, 1, &m_mutex->GetUAV());
		X_RETURN(m_uavTables[UAV_TABLE_MUTEX], descriptorTable->GetCbvSrvUavTable(m_descriptorTableCache.get()), false);
	}
#else
	{
		const auto descriptorTable = Util::DescriptorTable::MakeUnique();
		descriptorTable->SetDescriptors(0, 1, &m_grid->GetPackedUAV());
		X_RETURN(m_uavTables[UAV_TABLE_VOXELIZE], descriptorTable->GetCbvSrvUavTable(m_descriptorTableCache.get()), false);
	}
#endif

	{
		const auto descriptorTable = Util::DescriptorTable::MakeUnique();
		descriptorTable->SetDescriptors(0, 1, &m_KBufferDepth->GetUAV());
		X_RETURN(m_uavTables[UAV_TABLE_KBUFFER], descriptorTable->GetCbvSrvUavTable(m_descriptorTableCache.get()), false);
	}

	// Get SRV
	{
		const auto descriptorTable = Util::DescriptorTable::MakeUnique();
		descriptorTable->SetDescriptors(0, 1, &m_KBufferDepth->GetSRV());
		X_RETURN(m_srvTables[SRV_K_DEPTH], descriptorTable->GetCbvSrvUavTable(m_descriptorTableCache.get()), false);
	}

	// Get graphics pipeline layouts
	const auto numUAVs = USE_MUTEX ? 5u : 2u;
	{
		const auto utilPipelineLayout = Util::PipelineLayout::MakeUnique();
		utilPipelineLayout->SetRange(0, DescriptorType::CBV, 2, 0, 0, DescriptorFlag::DATA_STATIC);
		utilPipelineLayout->SetRange(1, DescriptorType::CBV, 1, 0, 0, DescriptorFlag::DATA_STATIC);
		utilPipelineLayout->SetRange(2, DescriptorType::UAV, numUAVs, 0, 0,
			DescriptorFlag::DATA_STATIC_WHILE_SET_AT_EXECUTE);
		utilPipelineLayout->SetRange(3, DescriptorType::SRV, static_cast<uint32_t>(size(srvs)), 0,
			0, DescriptorFlag::DATA_STATIC);
		utilPipelineLayout->SetShaderStage(0, Shader::Stage::VS);
		utilPipelineLayout->SetShaderStage(1, Shader::Stage::PS);
		utilPipelineLayout->SetShaderStage(2, Shader::Stage::PS);
		utilPipelineLayout->SetShaderStage(3, Shader::Stage::VS);
		X_RETURN(m_pipelineLayouts[PASS_VOXELIZE], utilPipelineLayout->GetPipelineLayout(
			m_pipelineLayoutCache.get(), PipelineLayoutFlag::NONE, L"VoxelizationPass"), false);
	}

	{
		const auto utilPipelineLayout = Util::PipelineLayout::MakeUnique();
		utilPipelineLayout->SetRange(0, DescriptorType::CBV, 1, 0, 0, DescriptorFlag::DATA_STATIC);
		utilPipelineLayout->SetRange(1, DescriptorType::CBV, 1, 0, 0, DescriptorFlag::DATA_STATIC);
		utilPipelineLayout->SetRange(2, DescriptorType::UAV, numUAVs, 0, 0,
			DescriptorFlag::DATA_STATIC_WHILE_SET_AT_EXECUTE);
		utilPipelineLayout->SetShaderStage(0, Shader::Stage::VS);
		utilPipelineLayout->SetShaderStage(1, Shader::Stage::PS);
		utilPipelineLayout->SetShaderStage(2, Shader::Stage::PS);
		X_RETURN(m_pipelineLayouts[PASS_VOXELIZE_UNION], utilPipelineLayout->GetPipelineLayout(
			m_pipelineLayoutCache.get(), PipelineLayoutFlag::ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT,
			L"VoxelizationByUnionPass"), false);
	}

	{
		const auto utilPipelineLayout = Util::PipelineLayout::MakeUnique();
		utilPipelineLayout->SetRange(0, DescriptorType::CBV, 1, 0, 0, DescriptorFlag::DATA_STATIC);
		utilPipelineLayout->SetRange(1, DescriptorType::CBV, 1, 0, 0, DescriptorFlag::DATA_STATIC);
		utilPipelineLayout->SetRange(2, DescriptorType::UAV, numUAVs, 0, 0,
			DescriptorFlag::DATA_STATIC_WHILE_SET_AT_EXECUTE);
		utilPipelineLayout->SetRange(3, DescriptorType::CBV, 1, 0, 0, DescriptorFlag::DATA_STATIC);
		utilPipelineLayout->SetShaderStage(0, Shader::Stage::VS);
		utilPipelineLayout->SetShaderStage(1, Shader::Stage::PS);
		utilPipelineLayout->SetShaderStage(2, Shader::Stage::PS);
		utilPipelineLayout->SetShaderStage(3, Shader::Stage::DS);
		X_RETURN(m_pipelineLayouts[PASS_VOXELIZE_TESS], utilPipelineLayout->GetPipelineLayout(
			m_pipelineLayoutCache.get(), PipelineLayoutFlag::ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT,
			L"VoxelizationByTessellationPass"), false);
	}

	// Get graphics pipelines
	{
		const auto state = Graphics::State::MakeUnique();
		state->SetPipelineLayout(m_pipelineLayouts[PASS_VOXELIZE]);
		state->SetShader(Shader::Stage::VS, m_shaderPool->GetShader(Shader::Stage::VS, VS_TRI_PROJ));
		state->SetShader(Shader::Stage::PS, m_shaderPool->GetShader(Shader::Stage::PS, PS_TRI_PROJ));
		state->DSSetState(Graphics::DepthStencilPreset::DEPTH_STENCIL_NONE, m_graphicsPipelineCache.get());
		state->IASetPrimitiveTopologyType(PrimitiveTopologyType::TRIANGLE);
		state->RSSetState(Graphics::RasterizerPreset::CULL_NONE, m_graphicsPipelineCache.get());
		state->OMSetNumRenderTargets(0);
		X_RETURN(m_pipelines[PASS_VOXELIZE], state->GetPipeline(m_graphicsPipelineCache.get(), L"Voxelization"), false);

		state->SetShader(Shader::Stage::PS, m_shaderPool->GetShader(Shader::Stage::PS, PS_TRI_PROJ_SOLID));
		X_RETURN(m_pipelines[PASS_VOXELIZE_SOLID], state->GetPipeline(m_graphicsPipelineCache.get(), L"VoxelizationSolid"), false);

		state->IASetInputLayout(m_pInputLayout);
		state->SetPipelineLayout(m_pipelineLayouts[PASS_VOXELIZE_UNION]);
		state->SetShader(Shader::Stage::VS, m_shaderPool->GetShader(Shader::Stage::VS, VS_TRI_PROJ_UNION));
		state->SetShader(Shader::Stage::PS, m_shaderPool->GetShader(Shader::Stage::PS, PS_TRI_PROJ_UNION));
		X_RETURN(m_pipelines[PASS_VOXELIZE_UNION], state->GetPipeline(m_graphicsPipelineCache.get(), L"VoxelizationUnion"), false);

		state->SetShader(Shader::Stage::PS, m_shaderPool->GetShader(Shader::Stage::PS, PS_TRI_PROJ_UNION_SOLID));
		X_RETURN(m_pipelines[PASS_VOXELIZE_UNION_SOLID], state->GetPipeline(m_graphicsPipelineCache.get(), L"VoxelizationUnionSolid"), false);

		state->SetPipelineLayout(m_pipelineLayouts[PASS_VOXELIZE_TESS]);
		state->SetShader(Shader::Stage::VS, m_shaderPool->GetShader(Shader::Stage::VS, VS_TRI_PROJ_TESS));
		state->SetShader(Shader::Stage::HS, m_shaderPool->GetShader(Shader::Stage::HS, HS_TRI_PROJ));
		state->SetShader(Shader::Stage::DS, m_shaderPool->GetShader(Shader::Stage::DS, DS_TRI_PROJ));
		state->SetShader(Shader::Stage::PS, m_shaderPool->GetShader(Shader::Stage::PS, PS_TRI_PROJ));
		state->IASetPrimitiveTopologyType(PrimitiveTopologyType::PATCH);
		X_RETURN(m_pipelines[PASS_VOXELIZE_TESS], state->GetPipeline(m_graphicsPipelineCache.get(), L"VoxelizationTess"), false);

		state->SetShader(Shader::Stage::PS, m_shaderPool->GetShader(Shader::Stage::PS, PS_TRI_PROJ_SOLID));
		X_RETURN(m_pipelines[PASS_VOXELIZE_TESS_SOLID], state->GetPipeline(m_graphicsPipelineCache.get(), L"VoxelizatioTessSolid"), false);
	}

	// Get compute pipeline layout
	{
		const auto utilPipelineLayout = Util::PipelineLayout::MakeUnique();
		utilPipelineLayout->SetRange(0, DescriptorType::CBV, 1, 0);
		utilPipelineLayout->SetRange(1, DescriptorType::SRV, USE_MUTEX ? 4 : 1, 0);
		utilPipelineLayout->SetRange(2, DescriptorType::UAV, 1, 0, 0, DescriptorFlag::DATA_STATIC_WHILE_SET_AT_EXECUTE);
		X_RETURN(m_pipelineLayouts[PASS_FILL_SOLID], utilPipelineLayout->GetPipelineLayout(
			m_pipelineLayoutCache.get(), PipelineLayoutFlag::NONE, L"SolidFillPass"), false);
	}

	// Get compute pipeline
	{
		const auto state = Compute::State::MakeUnique();
		state->SetPipelineLayout(m_pipelineLayouts[PASS_FILL_SOLID]);
		state->SetShader(m_shaderPool->GetShader(Shader::Stage::CS, CS_FILL_SOLID));
		X_RETURN(m_pipelines[PASS_FILL_SOLID], state->GetPipeline(m_computePipelineCache.get(), L"SolidFill"), false);
	}

	return true;
}

bool Voxelizer::prerenderBoxArray(Format rtFormat, Format dsFormat)
{
	// Get SRV
#if	USE_MUTEX
	{
		const Descriptor descriptors[] =
		{
			m_grid[1]->GetSRV(),
			m_grid[2]->GetSRV(),
			m_grid[3]->GetSRV()
		};
		const auto descriptorTable = Util::DescriptorTable::MakeUnique();
		descriptorTable->SetDescriptors(0, static_cast<uint32_t>(size(descriptors)), descriptors);
		X_RETURN(m_srvTables[SRV_TABLE_GRID_XYZ], descriptorTable->GetCbvSrvUavTable(m_descriptorTableCache.get()), false);
	}
#else
	if (!m_srvTables[SRV_TABLE_GRID])
	{
		const auto descriptorTable = Util::DescriptorTable::MakeUnique();
		descriptorTable->SetDescriptors(0, 1, &m_grid->GetSRV());
		X_RETURN(m_srvTables[SRV_TABLE_GRID], descriptorTable->GetCbvSrvUavTable(m_descriptorTableCache.get()), false);
	}
#endif

	for (uint8_t i = 0; i < FrameCount; ++i)
	{
		// Get CBV
		const auto descriptorTable = Util::DescriptorTable::MakeUnique();
		descriptorTable->SetDescriptors(0, 1, &m_cbMatrices->GetCBV(i));
		X_RETURN(m_cbvTables[CBV_TABLE_MATRICES + i], descriptorTable->GetCbvSrvUavTable(m_descriptorTableCache.get()), false);
	}

	// Get pipeline layout
	const auto utilPipelineLayout = Util::PipelineLayout::MakeUnique();
	utilPipelineLayout->SetRange(0, DescriptorType::CBV, 1, 0, 0, DescriptorFlag::DATA_STATIC);
	utilPipelineLayout->SetRange(1, DescriptorType::SRV, USE_MUTEX ? 4 : 1, 0);
	utilPipelineLayout->SetShaderStage(0, Shader::Stage::VS);
	utilPipelineLayout->SetShaderStage(1, Shader::Stage::VS);
	X_RETURN(m_pipelineLayouts[PASS_DRAW_AS_BOX], utilPipelineLayout->GetPipelineLayout(
		m_pipelineLayoutCache.get(), PipelineLayoutFlag::NONE, L"DrawAsBoxPass"), false);

	// Get pipeline
	const auto state = Graphics::State::MakeUnique();
	state->SetPipelineLayout(m_pipelineLayouts[PASS_DRAW_AS_BOX]);
	state->SetShader(Shader::Stage::VS, m_shaderPool->GetShader(Shader::Stage::VS, VS_BOX_ARRAY));
	state->SetShader(Shader::Stage::PS, m_shaderPool->GetShader(Shader::Stage::PS, PS_SIMPLE));
	state->IASetPrimitiveTopologyType(PrimitiveTopologyType::TRIANGLE);
	state->OMSetRTVFormats(&rtFormat, 1);
	state->OMSetDSVFormat(dsFormat);
	X_RETURN(m_pipelines[PASS_DRAW_AS_BOX], state->GetPipeline(m_graphicsPipelineCache.get(), L"DrawAsBox"), false);

	return true;
}

bool Voxelizer::prerayCast(Format rtFormat, Format dsFormat)
{
	for (uint8_t i = 0; i < FrameCount; ++i)
	{
		// Get CBV
		const auto descriptorTable = Util::DescriptorTable::MakeUnique();
		descriptorTable->SetDescriptors(0, 1, &m_cbPerObject->GetCBV(i));
		X_RETURN(m_cbvTables[CBV_TABLE_PER_OBJ + i], descriptorTable->GetCbvSrvUavTable(m_descriptorTableCache.get()), false);
	}

	// Get SRV
	if (!m_srvTables[SRV_TABLE_GRID])
	{
		const auto descriptorTable = Util::DescriptorTable::MakeUnique();
#if	USE_MUTEX
		descriptorTable->SetDescriptors(0, 1, &m_grid[0]->GetSRV());
#else
		descriptorTable->SetDescriptors(0, 1, &m_grid->GetSRV());
#endif
		X_RETURN(m_srvTables[SRV_TABLE_GRID], descriptorTable->GetCbvSrvUavTable(m_descriptorTableCache.get()), false);
	}

	// Create the sampler table
	const auto samplerTable = Util::DescriptorTable::MakeUnique();
	const auto sampler = LINEAR_CLAMP;
	samplerTable->SetSamplers(0, 1, &sampler, m_descriptorTableCache.get());
	X_RETURN(m_samplerTable, samplerTable->GetSamplerTable(m_descriptorTableCache.get()), false);

	// Get pipeline layout
	const auto utilPipelineLayout = Util::PipelineLayout::MakeUnique();
	utilPipelineLayout->SetRange(0, DescriptorType::CBV, 1, 0, 0, DescriptorFlag::DATA_STATIC);
	utilPipelineLayout->SetRange(1, DescriptorType::SRV, 1, 0);
	utilPipelineLayout->SetRange(2, DescriptorType::SAMPLER, 1, 0);
	utilPipelineLayout->SetShaderStage(0, Shader::Stage::PS);
	utilPipelineLayout->SetShaderStage(1, Shader::Stage::PS);
	utilPipelineLayout->SetShaderStage(2, Shader::Stage::PS);
	X_RETURN(m_pipelineLayouts[PASS_RAY_CAST], utilPipelineLayout->GetPipelineLayout(
		m_pipelineLayoutCache.get(), PipelineLayoutFlag::NONE, L"RayCastPass"), false);

	// Get pipeline
	const auto state = Graphics::State::MakeUnique();
	state->SetPipelineLayout(m_pipelineLayouts[PASS_RAY_CAST]);
	state->SetShader(Shader::Stage::VS, m_shaderPool->GetShader(Shader::Stage::VS, VS_SCREEN_QUAD));
	state->SetShader(Shader::Stage::PS, m_shaderPool->GetShader(Shader::Stage::PS, PS_RAY_CAST));
	state->DSSetState(Graphics::DepthStencilPreset::DEPTH_STENCIL_NONE, m_graphicsPipelineCache.get());
	state->IASetPrimitiveTopologyType(PrimitiveTopologyType::TRIANGLE);
	state->OMSetRTVFormats(&rtFormat, 1);
	X_RETURN(m_pipelines[PASS_RAY_CAST], state->GetPipeline(m_graphicsPipelineCache.get(), L"RayCast"), false);

	return true;
}

void Voxelizer::voxelize(const CommandList* pCommandList, Method voxMethod, bool depthPeel, uint8_t mipLevel)
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
#if	USE_MUTEX
	ResourceBarrier barriers[4];
	auto numBarriers = 0u;
	for (uint8_t i = 1; i < 4; ++i)
		numBarriers = m_grid[i]->SetBarrier(barriers, ResourceState::UNORDERED_ACCESS, numBarriers);
#else
	ResourceBarrier barriers[1];
	auto numBarriers = m_grid->SetBarrier(barriers, ResourceState::UNORDERED_ACCESS);
#endif
	pCommandList->Barrier(numBarriers, barriers);
	if (depthPeel) m_KBufferDepth->SetBarrier(barriers, ResourceState::UNORDERED_ACCESS); // Implicit state promotion

	// Set descriptor tables
	pCommandList->SetGraphicsPipelineLayout(m_pipelineLayouts[layoutIdx]);
	pCommandList->SetGraphicsDescriptorTable(0, m_cbvTables[CBV_TABLE_VOXELIZE]);
	pCommandList->SetGraphicsDescriptorTable(1, m_cbvTables[CBV_TABLE_PER_MIP]);
	pCommandList->SetGraphicsDescriptorTable(2, m_uavTables[UAV_TABLE_VOXELIZE + USE_MUTEX]);
	switch (voxMethod)
	{
	case TRI_PROJ:
		pCommandList->SetGraphicsDescriptorTable(3, m_srvTables[SRV_TABLE_VB_IB]);
		break;
	case TRI_PROJ_TESS:
		pCommandList->SetGraphicsDescriptorTable(3, m_cbvTables[CBV_TABLE_PER_MIP]);
		break;
	}

	// Set pipeline state
	pCommandList->SetPipelineState(m_pipelines[pipeIdx]);

	// Set viewport
	const auto gridSize = GRID_SIZE >> mipLevel;
	const auto fGridSize = static_cast<float>(gridSize);
	Viewport viewport(0.0f, 0.0f, fGridSize, fGridSize);
	RectRange scissorRect(0, 0, gridSize, gridSize);
	pCommandList->RSSetViewports(1, &viewport);
	pCommandList->RSSetScissorRects(1, &scissorRect);

	// Record commands.
#if	USE_MUTEX
	for (uint8_t i = 1; i < 4; ++i)
		pCommandList->ClearUnorderedAccessViewFloat(m_uavTables[UAV_TABLE_VOXELIZE + i], m_grid[i]->GetUAV(), m_grid[i].get(), XMVECTORF32{ 0.0f }.f);
#else
	pCommandList->ClearUnorderedAccessViewUint(m_uavTables[UAV_TABLE_VOXELIZE], m_grid->GetPackedUAV(), m_grid.get(), XMVECTORU32{ 0 }.u);
#endif
	if (depthPeel) pCommandList->ClearUnorderedAccessViewUint(m_uavTables[UAV_TABLE_KBUFFER], m_KBufferDepth->GetUAV(),
		m_KBufferDepth.get(), XMVECTORU32{ UINT32_MAX }.u);

	// Set IA
	if (voxMethod != TRI_PROJ)
	{
		pCommandList->IASetVertexBuffers(0, 1, &m_vertexBuffer->GetVBV());
		pCommandList->IASetIndexBuffer(m_indexbuffer->GetIBV());
	}

	pCommandList->IASetPrimitiveTopology(voxMethod == TRI_PROJ_TESS ?
		PrimitiveTopology::CONTROL_POINT3_PATCHLIST : PrimitiveTopology::TRIANGLELIST);

	if (voxMethod == TRI_PROJ)
		pCommandList->Draw(3, instanceCount, 0, 0);
	else
		pCommandList->DrawIndexed(m_numIndices, instanceCount, 0, 0, 0);
}

void Voxelizer::voxelizeSolid(const CommandList* pCommandList, Method voxMethod, uint8_t mipLevel)
{
	// Surface voxelization with depth peeling
	voxelize(pCommandList, voxMethod, true, mipLevel);

	// Set resource barriers
#if	USE_MUTEX
	ResourceBarrier barriers[5];
	auto numBarriers = m_grid[0]->SetBarrier(barriers, ResourceState::UNORDERED_ACCESS);
	for (uint8_t i = 1; i < 4; ++i)
		numBarriers = m_grid[i]->SetBarrier(barriers, ResourceState::NON_PIXEL_SHADER_RESOURCE, numBarriers);
#else
	ResourceBarrier barriers[2];
	auto numBarriers = m_grid->SetBarrier(barriers, ResourceState::UNORDERED_ACCESS);
#endif
	numBarriers = m_KBufferDepth->SetBarrier(barriers, ResourceState::NON_PIXEL_SHADER_RESOURCE, numBarriers);
	pCommandList->Barrier(numBarriers, barriers);

#if	USE_MUTEX
	pCommandList->ClearUnorderedAccessViewFloat(m_uavTables[UAV_TABLE_VOXELIZE], m_grid[0]->GetUAV(), m_grid[0].get(), XMVECTORF32{ 0.0f }.f);
#endif

	// Set descriptor tables
	pCommandList->SetComputePipelineLayout(m_pipelineLayouts[PASS_FILL_SOLID]);

	pCommandList->SetComputeDescriptorTable(0, m_cbvTables[CBV_TABLE_PER_MIP]);
	pCommandList->SetComputeDescriptorTable(1, m_srvTables[SRV_K_DEPTH]);
	pCommandList->SetComputeDescriptorTable(2, m_uavTables[UAV_TABLE_VOXELIZE]);

	// Set pipeline state
	pCommandList->SetPipelineState(m_pipelines[PASS_FILL_SOLID]);

	// Record commands.
	const auto numGroups = DIV_UP(GRID_SIZE, 4);
	pCommandList->Dispatch(numGroups, numGroups, numGroups);
}

void Voxelizer::renderBoxArray(const CommandList* pCommandList, uint8_t frameIndex, const Descriptor& rtv, const Descriptor& dsv)
{
	// Set resource barrier
#if	USE_MUTEX
	ResourceBarrier barriers[3];
	auto numBarriers = 0u;
	for (uint8_t i = 1; i < 4; ++i)
		numBarriers = m_grid[i]->SetBarrier(barriers, ResourceState::NON_PIXEL_SHADER_RESOURCE, numBarriers);
	pCommandList->Barrier(numBarriers, barriers);
#else
	ResourceBarrier barrier;
	const auto numBarriers = m_grid->SetBarrier(&barrier, ResourceState::NON_PIXEL_SHADER_RESOURCE);
	pCommandList->Barrier(numBarriers, &barrier);
#endif

	// Set descriptor tables
	pCommandList->SetGraphicsPipelineLayout(m_pipelineLayouts[PASS_DRAW_AS_BOX]);

	pCommandList->SetGraphicsDescriptorTable(0, m_cbvTables[CBV_TABLE_MATRICES + frameIndex]);
	pCommandList->SetGraphicsDescriptorTable(1, m_srvTables[SRV_TABLE_GRID + USE_MUTEX]);

	// Set pipeline state
	pCommandList->SetPipelineState(m_pipelines[PASS_DRAW_AS_BOX]);

	// Set viewport
	const auto gridSize = GRID_SIZE >> SHOW_MIP;
	Viewport viewport(0.0f, 0.0f, m_viewport.x, m_viewport.y);
	RectRange scissorRect(0, 0, static_cast<long>(m_viewport.x), static_cast<long>(m_viewport.y));
	pCommandList->RSSetViewports(1, &viewport);
	pCommandList->RSSetScissorRects(1, &scissorRect);

	pCommandList->OMSetRenderTargets(1, &rtv, &dsv);

	// Record commands.
	pCommandList->IASetPrimitiveTopology(PrimitiveTopology::TRIANGLESTRIP);
	pCommandList->Draw(4, 6 * gridSize * gridSize * gridSize, 0, 0);
}

void Voxelizer::renderRayCast(const CommandList* pCommandList, uint8_t frameIndex, const Descriptor& rtv, const Descriptor& dsv)
{
	// Set resource barriers
	ResourceBarrier barrier;
#if	USE_MUTEX
	const auto numBarriers = m_grid[0]->SetBarrier(&barrier, ResourceState::PIXEL_SHADER_RESOURCE);
#else
	const auto numBarriers = m_grid->SetBarrier(&barrier, ResourceState::PIXEL_SHADER_RESOURCE);
#endif
	pCommandList->Barrier(numBarriers, &barrier);

	// Set descriptor tables
	pCommandList->SetGraphicsPipelineLayout(m_pipelineLayouts[PASS_RAY_CAST]);

	pCommandList->SetGraphicsDescriptorTable(0, m_cbvTables[CBV_TABLE_PER_OBJ + frameIndex]);
	pCommandList->SetGraphicsDescriptorTable(1, m_srvTables[SRV_TABLE_GRID]);
	pCommandList->SetGraphicsDescriptorTable(2, m_samplerTable);

	// Set pipeline state
	pCommandList->SetPipelineState(m_pipelines[PASS_RAY_CAST]);

	// Set viewport
	Viewport viewport(0.0f, 0.0f, m_viewport.x, m_viewport.y);
	RectRange scissorRect(0, 0, static_cast<long>(m_viewport.x), static_cast<long>(m_viewport.y));
	pCommandList->RSSetViewports(1, &viewport);
	pCommandList->RSSetScissorRects(1, &scissorRect);

	pCommandList->OMSetRenderTargets(1, &rtv);

	// Record commands.
	pCommandList->IASetPrimitiveTopology(PrimitiveTopology::TRIANGLESTRIP);
	pCommandList->Draw(3, 1, 0, 0);
}
