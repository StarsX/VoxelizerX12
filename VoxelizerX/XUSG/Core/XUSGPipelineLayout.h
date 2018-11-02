//--------------------------------------------------------------------------------------
// By Stars XU Tianchen
//--------------------------------------------------------------------------------------

#pragma once

#include "XUSGShader.h"
#include "XUSGDescriptor.h"

namespace XUSG
{
	enum class DescriptorType : uint8_t
	{
		SRV,
		UAV,
		CBV,
		SAMPLER,

		NUM
	};

	namespace Graphics
	{
		namespace Pipeline
		{
			class Pool;
		}
	}

	class PipelineLayoutPool;

	struct DescriptorRange
	{
		DescriptorType ViewType;
		Shader::Stage::Type ShaderStage;
		uint32_t NumDescriptors;
		uint32_t BaseRegister;
		uint32_t Space;
	};

	namespace Util
	{
		class PipelineLayout
		{
		public:
			PipelineLayout();
			virtual ~PipelineLayout();

			void SetShaderStage(uint32_t index, Shader::Stage::Type stage);
			void SetRange(uint32_t index, DescriptorType type, uint32_t num, uint32_t baseReg, uint32_t space = 0);

			XUSG::PipelineLayout CreatePipelineLayout(PipelineLayoutPool &pipelineLayoutPool, uint8_t flags);
			XUSG::PipelineLayout CreatePipelineLayout(Graphics::Pipeline::Pool &pipelinePool, uint8_t flags);
			XUSG::PipelineLayout GetPipelineLayout(PipelineLayoutPool &pipelineLayoutPool, uint8_t flags);
			XUSG::PipelineLayout GetPipelineLayout(Graphics::Pipeline::Pool &pipelinePool, uint8_t flags);

			DescriptorTableLayout CreateDescriptorTableLayout(uint32_t index, PipelineLayoutPool &pipelineLayoutPool) const;
			DescriptorTableLayout CreateDescriptorTableLayout(uint32_t index, Graphics::Pipeline::Pool &pipelinePool) const;
			DescriptorTableLayout GetDescriptorTableLayout(uint32_t index, PipelineLayoutPool &pipelineLayoutPool) const;
			DescriptorTableLayout GetDescriptorTableLayout(uint32_t index, Graphics::Pipeline::Pool &pipelinePool) const;

		protected:
			friend class PipelineLayoutPool;

			void setPipelineLayoutKey(PipelineLayoutPool &pipelineLayoutPool, uint8_t flags);
			std::string &checkKeySpace(uint32_t index);

			std::vector<std::string> m_descriptorTableLayoutKeys;
			std::string m_pipelineLayoutKey;
		};
	}

	class PipelineLayoutPool
	{
	public:
		PipelineLayoutPool();
		PipelineLayoutPool(const Device &device);
		virtual ~PipelineLayoutPool();

		void SetDevice(const Device &device);

		PipelineLayout GetPipelineLayout(Util::PipelineLayout &util, uint8_t flags);

		DescriptorTableLayout CreateDescriptorTableLayout(uint32_t index, const Util::PipelineLayout &util);
		DescriptorTableLayout GetDescriptorTableLayout(uint32_t index, const Util::PipelineLayout &util);

	protected:
		friend class Util::PipelineLayout;

		PipelineLayout createPipelineLayout(const std::string &key) const;
		PipelineLayout getPipelineLayout(const std::string &key);

		DescriptorTableLayout createDescriptorTableLayout(const std::string &key);
		DescriptorTableLayout getDescriptorTableLayout(const std::string &key);

		std::unordered_map<std::string, PipelineLayout> m_pipelineLayouts;
		std::unordered_map<std::string, DescriptorTableLayout> m_descriptorTableLayouts;

		Device m_device;
	};
}
