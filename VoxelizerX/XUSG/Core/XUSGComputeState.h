//--------------------------------------------------------------------------------------
// By Stars XU Tianchen
//--------------------------------------------------------------------------------------

#pragma once

#include "XUSGPipelineLayout.h"

namespace XUSG
{
	namespace Compute
	{
		namespace Pipeline
		{
			class Pool;
		}

		class State
		{
		public:
			State();
			virtual ~State();

			void SetPipelineLayout(const PipelineLayout &layout);
			void SetShader(Blob shader);

			PipelineState CreatePipeline(Pipeline::Pool &pipelinePool) const;
			PipelineState GetPipeline(Pipeline::Pool &pipelinePool) const;

		protected:
			friend class Pipeline::Pool;

			struct Key
			{
				void	*PipelineLayout;
				void	*Shader;
			};

			Key *m_pKey;
			std::string m_key;
		};

		namespace Pipeline
		{
			class Pool
			{
			public:
				Pool();
				Pool(const Device &device);
				virtual ~Pool();

				void SetDevice(const Device &device);

				PipelineLayout GetPipelineLayout(Util::PipelineLayout &util, uint8_t flags);
				DescriptorTableLayout CreateDescriptorTableLayout(uint32_t index, const Util::PipelineLayout &util);
				DescriptorTableLayout GetDescriptorTableLayout(uint32_t index, const Util::PipelineLayout &util);

				PipelineLayoutPool &GetPipelineLayoutPool();

				PipelineState GetPipeline(const State &state);

			protected:
				friend class State;

				PipelineState createPipeline(const State::Key *pKey);
				PipelineState getPipeline(const std::string &key);

				Device m_device;

				PipelineLayoutPool	m_pipelineLayoutPool;

				std::unordered_map<std::string, PipelineState> m_pipelines;
			};
		}
	}
}
