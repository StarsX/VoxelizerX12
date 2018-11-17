//--------------------------------------------------------------------------------------
// By Stars XU Tianchen
//--------------------------------------------------------------------------------------

#include "DXFrameworkHelper.h"
#include "XUSGComputeState.h"

using namespace std;
using namespace XUSG;
using namespace Compute;
using namespace Pipeline;

State::State()
{
	// Default state
	m_key.resize(sizeof(Key));
	m_pKey = reinterpret_cast<Key*>(&m_key[0]);
	memset(m_pKey, 0, sizeof(Key));
}

State::~State()
{
}

void State::SetPipelineLayout(const PipelineLayout &layout)
{
	m_pKey->PipelineLayout = layout.Get();
}

void State::SetShader(Blob shader)
{
	m_pKey->Shader = shader.Get();
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
	m_pipelines()
{
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

PipelineState Pool::createPipeline(const State::Key *pKey)
{
	// Fill desc
	PipelineDesc desc = {};
	if (pKey->PipelineLayout)
		desc.pRootSignature = static_cast<decltype(desc.pRootSignature)>(pKey->PipelineLayout);

	if (pKey->Shader)
		desc.CS = Shader::ByteCode(static_cast<BlobType*>(pKey->Shader));

	// Create pipeline
	PipelineState pipeline;
	V_RETURN(m_device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&pipeline)), cerr, nullptr);

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
