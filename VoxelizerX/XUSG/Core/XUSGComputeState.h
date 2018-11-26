//--------------------------------------------------------------------------------------
// By Stars XU Tianchen
//--------------------------------------------------------------------------------------

#pragma once

#include "XUSGPipelineLayout.h"

namespace XUSG
{
	namespace Compute
	{
		class PipelineCache;

		class State
		{
		public:
			struct Key
			{
				void *PipelineLayout;
				void *Shader;
			};

			State();
			virtual ~State();

			void SetPipelineLayout(const PipelineLayout &layout);
			void SetShader(Blob shader);

			Pipeline CreatePipeline(PipelineCache &pipelineCache) const;
			Pipeline GetPipeline(PipelineCache &pipelineCache) const;

			const std::string &GetKey() const;

		protected:
			Key *m_pKey;
			std::string m_key;
		};

		class PipelineCache
		{
		public:
			PipelineCache();
			PipelineCache(const Device &device);
			virtual ~PipelineCache();

			void SetDevice(const Device &device);
			void SetPipeline(const std::string &key, const Pipeline &pipeline);

			Pipeline CreatePipeline(const State &state);
			Pipeline GetPipeline(const State &state);

		protected:
			Pipeline createPipeline(const State::Key *pKey);
			Pipeline getPipeline(const std::string &key);

			Device m_device;

			std::unordered_map<std::string, Pipeline> m_pipelines;
		};
	}
}
