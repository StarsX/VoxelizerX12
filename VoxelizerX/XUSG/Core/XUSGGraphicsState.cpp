//--------------------------------------------------------------------------------------
// By Stars XU Tianchen
//--------------------------------------------------------------------------------------

#include "DXFrameworkHelper.h"
#include "XUSGGraphicsState.h"
#include "XUSGBlend.inl"
#include "XUSGRasterizer.inl"
#include "XUSGDepthStencil.inl"

using namespace XUSG;
using namespace Graphics;
using namespace Pipeline;

State::State()
{
	// Default state
	m_key.resize(sizeof(Key));
	m_pKey = reinterpret_cast<Key*>(&m_key[0]);
	memset(m_pKey, 0, sizeof(Key));
	m_pKey->PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	m_pKey->SampleCount = 1;
}

State::~State()
{
}

void State::SetPipelineLayout(const PipelineLayout &layout)
{
	m_pKey->PipelineLayout = layout.Get();
}

void State::SetShader(Shader::Stage::Type stage, Blob shader)
{
	m_pKey->Shaders[stage] = shader.Get();
}

void State::OMSetBlendState(const Blend &blend)
{
	m_pKey->Blend = blend.get();
}

void State::RSSetState(const Rasterizer &rasterizer)
{
	m_pKey->Rasterizer = rasterizer.get();
}

void State::DSSetState(const DepthStencil &depthStencil)
{
	m_pKey->DepthStencil = depthStencil.get();
}

void State::OMSetBlendState(BlendPreset::Type preset, Pipeline::Pool &pipelinePool)
{
	OMSetBlendState(pipelinePool.GetBlend(preset));
}

void State::RSSetState(RasterizerPreset::Type preset, Pipeline::Pool &pipelinePool)
{
	RSSetState(pipelinePool.GetRasterizer(preset));
}

void State::DSSetState(DepthStencilPreset::Type preset, Pipeline::Pool &pipelinePool)
{
	DSSetState(pipelinePool.GetDepthStencil(preset));
}

void State::IASetInputLayout(const InputLayout &layout)
{
	m_pKey->InputLayout = layout.get();
}

void State::IASetPrimitiveTopologyType(PrimitiveTopologyType type)
{
	m_pKey->PrimitiveTopologyType = type;
}

void State::OMSetNumRenderTargets(uint8_t n)
{
	m_pKey->NumRenderTargets = n;
}

void State::OMSetRTVFormat(uint8_t i, Format format)
{
	m_pKey->RTVFormats[i] = format;
}

void State::OMSetRTVFormats(const Format *formats, uint8_t n)
{
	OMSetNumRenderTargets(n);

	for (auto i = 0u; i < n; ++i)
		OMSetRTVFormat(i, formats[i]);
}

void State::OMSetDSVFormat(Format format)
{
	m_pKey->DSVFormat = format;
}

PipelineState State::CreatePipeline(Pipeline::Pool &pipelinePool) const
{
	return pipelinePool.createPipeline(m_pKey);
}

PipelineState State::GetPipeline(Pipeline::Pool &pipelinePool) const
{
	return pipelinePool.getPipeline(m_key);
}

//--------------------------------------------------------------------------------------

Pool::Pool() :
	m_device(nullptr),
	m_pipelines(),
	m_blends(),
	m_rasterizers(),
	m_depthStencils()
{
	// Blend states
	m_pfnBlends[BlendPreset::DEFAULT_OPAQUE] = DefaultOpaque;
	m_pfnBlends[BlendPreset::PREMULTIPLITED] = Premultiplied;
	m_pfnBlends[BlendPreset::NON_PRE_MUL] = NonPremultiplied;
	m_pfnBlends[BlendPreset::NON_PREMUL_RT0] = NonPremultipliedRT0;
	m_pfnBlends[BlendPreset::ALPHA_TO_COVERAGE] = AlphaToCoverage;
	m_pfnBlends[BlendPreset::ACCUMULATIVE] = Accumulative;
	m_pfnBlends[BlendPreset::AUTO_NON_PREMUL] = AutoNonPremultiplied;
	m_pfnBlends[BlendPreset::ZERO_ALPHA_PREMUL] = ZeroAlphaNonPremultiplied;
	m_pfnBlends[BlendPreset::MULTIPLITED] = Multiplied;
	m_pfnBlends[BlendPreset::WEIGHTED] = Weighted;
	m_pfnBlends[BlendPreset::SELECT_MIN] = SelectMin;
	m_pfnBlends[BlendPreset::SELECT_MAX] = SelectMax;

	// Rasterizer states
	m_pfnRasterizers[RasterizerPreset::CULL_BACK] = CullBack;
	m_pfnRasterizers[RasterizerPreset::CULL_NONE] = CullNone;
	m_pfnRasterizers[RasterizerPreset::CULL_FRONT] = CullFront;
	m_pfnRasterizers[RasterizerPreset::FILL_WIREFRAME] = CullWireframe;

	// Depth stencil states
	m_pfnDepthStencils[DepthStencilPreset::DEFAULT] = DepthStencilDefault;
	m_pfnDepthStencils[DepthStencilPreset::DEPTH_STENCIL_NONE] = DepthStencilNone;
	m_pfnDepthStencils[DepthStencilPreset::DEPTH_READ_LESS] = DepthRead;
	m_pfnDepthStencils[DepthStencilPreset::DEPTH_READ_LESS_EQUAL] = DepthReadLessEqual;
	m_pfnDepthStencils[DepthStencilPreset::DEPTH_READ_EQUAL] = DepthReadEqual;
}

Pool::Pool(const Device &device) :
	Pool()
{
	SetDevice(device);
}

Pool::~Pool()
{
}

void Pool::SetDevice(const Device &device)
{
	m_device = device;
	m_pipelineLayoutPool.SetDevice(device);
}

void Pool::SetInputLayout(uint32_t index, const InputElementTable &elementTable)
{
	m_inputLayoutPool.SetLayout(index, elementTable);
}

InputLayout Pool::GetInputLayout(uint32_t index) const
{
	return m_inputLayoutPool.GetLayout(index);
}

InputLayout Pool::CreateInputLayout(const InputElementTable &elementTable)
{
	return m_inputLayoutPool.CreateLayout(elementTable);;
}

PipelineLayout Pool::GetPipelineLayout(Util::PipelineLayout &util, uint8_t flags)
{
	return m_pipelineLayoutPool.GetPipelineLayout(util, flags);
}

DescriptorTableLayout Pool::CreateDescriptorTableLayout(uint32_t index, const Util::PipelineLayout &util)
{
	return m_pipelineLayoutPool.CreateDescriptorTableLayout(index, util);
}

DescriptorTableLayout Pool::GetDescriptorTableLayout(uint32_t index, const Util::PipelineLayout &util)
{
	return m_pipelineLayoutPool.GetDescriptorTableLayout(index, util);
}

PipelineLayoutPool &Pool::GetPipelineLayoutPool()
{
	return m_pipelineLayoutPool;
}

PipelineState Pool::GetPipeline(const State &state)
{
	return getPipeline(state.m_key);
}

const Blend &Pool::GetBlend(BlendPreset::Type preset)
{
	if (m_blends[preset] == nullptr)
		m_blends[preset] = m_pfnBlends[preset]();

	return m_blends[preset];
}

const Rasterizer &Pool::GetRasterizer(RasterizerPreset::Type preset)
{
	if (m_rasterizers[preset] == nullptr)
		m_rasterizers[preset] = m_pfnRasterizers[preset]();

	return m_rasterizers[preset];
}

const DepthStencil &Pool::GetDepthStencil(DepthStencilPreset::Type preset)
{
	if (m_depthStencils[preset] == nullptr)
		m_depthStencils[preset] = m_pfnDepthStencils[preset]();

	return m_depthStencils[preset];
}

PipelineState Pool::createPipeline(const State::Key *pKey)
{
	// Fill desc
	PipelineDesc desc = {};
	if (pKey->PipelineLayout)
		desc.pRootSignature = static_cast<decltype(desc.pRootSignature)>(pKey->PipelineLayout);

	if (pKey->Shaders[Shader::Stage::VS])
		desc.VS = Shader::ByteCode(static_cast<BlobType*>(pKey->Shaders[Shader::Stage::VS]));
	if (pKey->Shaders[Shader::Stage::PS])
		desc.PS = Shader::ByteCode(static_cast<BlobType*>(pKey->Shaders[Shader::Stage::PS]));
	if (pKey->Shaders[Shader::Stage::DS])
		desc.DS = Shader::ByteCode(static_cast<BlobType*>(pKey->Shaders[Shader::Stage::DS]));
	if (pKey->Shaders[Shader::Stage::HS])
		desc.HS = Shader::ByteCode(static_cast<BlobType*>(pKey->Shaders[Shader::Stage::HS]));
	if (pKey->Shaders[Shader::Stage::GS])
		desc.GS = Shader::ByteCode(static_cast<BlobType*>(pKey->Shaders[Shader::Stage::GS]));

	const auto blend = static_cast<decltype(desc.BlendState)*>(pKey->Blend);
	desc.BlendState = *(blend ? blend : GetBlend(BlendPreset::DEFAULT_OPAQUE).get());
	desc.SampleMask = UINT_MAX;

	const auto rasterizer = static_cast<decltype(desc.RasterizerState)*>(pKey->Rasterizer);
	const auto depthStencil = static_cast<decltype(desc.DepthStencilState)*>(pKey->DepthStencil);
	desc.RasterizerState = *(rasterizer ? rasterizer : GetRasterizer(RasterizerPreset::CULL_BACK).get());
	desc.DepthStencilState = *(depthStencil ? depthStencil : GetDepthStencil(DepthStencilPreset::DEFAULT).get());
	if (pKey->InputLayout)
		desc.InputLayout = *static_cast<decltype(desc.InputLayout)*>(pKey->InputLayout);
	desc.PrimitiveTopologyType = static_cast<PrimitiveTopologyType>(pKey->PrimitiveTopologyType);
	desc.NumRenderTargets = pKey->NumRenderTargets;

	for (auto i = 0; i < 8; ++i)
		desc.RTVFormats[i] = static_cast<Format>(pKey->RTVFormats[i]);
	desc.DSVFormat = static_cast<Format>(pKey->DSVFormat);
	desc.SampleDesc.Count = pKey->SampleCount;

	// Create pipeline
	PipelineState pipeline;
	V_RETURN(m_device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipeline)), cerr, nullptr);

	return pipeline;
}

PipelineState Pool::getPipeline(const string &key)
{
	const auto pPipeline = m_pipelines.find(key);

	// Create one, if it does not exist
	if (pPipeline == m_pipelines.end())
	{
		const auto pKey = reinterpret_cast<const State::Key*>(key.data());
		const auto pipeline = createPipeline(pKey);
		m_pipelines[key] = pipeline;

		return pipeline;
	}

	return pPipeline->second;
}
