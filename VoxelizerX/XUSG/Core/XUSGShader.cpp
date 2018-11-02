//--------------------------------------------------------------------------------------
// By Stars XU Tianchen
//--------------------------------------------------------------------------------------

#include "DXFrameworkHelper.h"
#include "XUSGShader.h"

using namespace std;
using namespace XUSG;
using namespace Shader;

Pool::Pool() :
	m_shaders(),
	m_reflectors()
{
}

Pool::~Pool()
{
}

void Pool::SetShader(Stage::Type stage, uint32_t index, const Blob &shader)
{
	checkShaderStorage(stage, index) = shader;
}

void Pool::SetShader(Stage::Type stage, uint32_t index, const Blob &shader, const Reflector &reflector)
{
	SetShader(stage, index, shader);
	SetReflector(stage, index, reflector);
}

void Pool::SetReflector(Stage::Type stage, uint32_t index, const Reflector &reflector)
{
	checkReflectorStorage(stage, index) = reflector;
}

const Blob &Pool::CreateShader(Stage::Type stage, uint32_t index, const wstring &fileName)
{
	auto &shader = checkShaderStorage(stage, index);
	ThrowIfFailed(D3DReadFileToBlob(fileName.c_str(), &shader));

	auto &reflector = checkReflectorStorage(stage, index);
	ThrowIfFailed(D3DReflect(shader->GetBufferPointer(), shader->GetBufferSize(),
		IID_ID3D12ShaderReflection, &reflector));

	return shader;
}

Blob Pool::GetShader(Stage::Type stage, uint32_t index) const
{
	return index < m_shaders[stage].size() ? m_shaders[stage][index] : nullptr;
}

Reflector Pool::GetReflector(Stage::Type stage, uint32_t index) const
{
	return index < m_reflectors[stage].size() ? m_reflectors[stage][index] : nullptr;
}

Blob &Pool::checkShaderStorage(Stage::Type stage, uint32_t index)
{
	if (index >= m_shaders[stage].size())
		m_shaders[stage].resize(index + 1);

	return m_shaders[stage][index];
}

Reflector &Pool::checkReflectorStorage(Stage::Type stage, uint32_t index)
{
	if (index >= m_reflectors[stage].size())
		m_reflectors[stage].resize(index + 1);

	return m_reflectors[stage][index];
}
