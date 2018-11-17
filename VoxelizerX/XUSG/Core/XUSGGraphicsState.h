//--------------------------------------------------------------------------------------
// By Stars XU Tianchen
//--------------------------------------------------------------------------------------

#pragma once

#include "XUSGPipelineLayout.h"
#include "XUSGInputLayout.h"

namespace XUSG
{
	namespace Graphics
	{
		struct BlendPreset
		{
			enum Type : uint8_t
			{
				DEFAULT_OPAQUE,
				PREMULTIPLITED,
				ADDTIVE,
				NON_PRE_MUL,
				NON_PREMUL_RT0,
				ALPHA_TO_COVERAGE,
				ACCUMULATIVE,
				AUTO_NON_PREMUL,
				ZERO_ALPHA_PREMUL,
				MULTIPLITED,
				WEIGHTED,
				SELECT_MIN,
				SELECT_MAX,

				NUM
			};
		};

		struct RasterizerPreset
		{
			enum Type : uint8_t
			{
				CULL_BACK,
				CULL_NONE,
				CULL_FRONT,
				FILL_WIREFRAME,

				NUM
			};
		};

		struct DepthStencilPreset
		{
			enum Type : uint8_t
			{
				DEFAULT,
				DEPTH_STENCIL_NONE,
				DEPTH_READ_LESS,
				DEPTH_READ_LESS_EQUAL,
				DEPTH_READ_EQUAL,

				NUM
			};
		};

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
			void SetShader(Shader::Stage::Type stage, Blob shader);
			
			void OMSetBlendState(const Blend &blend);
			void RSSetState(const Rasterizer &rasterizer);
			void DSSetState(const DepthStencil &depthStencil);

			void OMSetBlendState(BlendPreset::Type preset, Pipeline::Pool &pipelinePool);
			void RSSetState(RasterizerPreset::Type preset, Pipeline::Pool &pipelinePool);
			void DSSetState(DepthStencilPreset::Type preset, Pipeline::Pool &pipelinePool);

			void IASetInputLayout(const InputLayout &layout);
			void IASetPrimitiveTopologyType(PrimitiveTopologyType type);

			void OMSetNumRenderTargets(uint8_t n);
			void OMSetRTVFormat(uint8_t i, Format format);
			void OMSetRTVFormats(const Format *formats, uint8_t n);
			void OMSetDSVFormat(Format format);

			PipelineState CreatePipeline(Pipeline::Pool &pipelinePool) const;
			PipelineState GetPipeline(Pipeline::Pool &pipelinePool) const;

		protected:
			friend class Pipeline::Pool;

			struct Key
			{
				void	*PipelineLayout;
				void	*Shaders[Shader::Stage::NUM_GRAPHICS];
				void	*Blend;
				void	*Rasterizer;
				void	*DepthStencil;
				void	*InputLayout;
				uint8_t	PrimitiveTopologyType;
				uint8_t	NumRenderTargets;
				uint8_t	RTVFormats[8];
				uint8_t	DSVFormat;
				uint8_t	SampleCount;
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

				void SetInputLayout(uint32_t index, const InputElementTable &elementTable);
				InputLayout GetInputLayout(uint32_t index) const;
				InputLayout CreateInputLayout(const InputElementTable &elementTable);

				PipelineLayout GetPipelineLayout(Util::PipelineLayout &util, uint8_t flags);
				DescriptorTableLayout CreateDescriptorTableLayout(uint32_t index, const Util::PipelineLayout &util);
				DescriptorTableLayout GetDescriptorTableLayout(uint32_t index, const Util::PipelineLayout &util);
				
				PipelineLayoutPool &GetPipelineLayoutPool();

				PipelineState GetPipeline(const State &state);

				const Blend			&GetBlend(BlendPreset::Type preset);
				const Rasterizer	&GetRasterizer(RasterizerPreset::Type preset);
				const DepthStencil	&GetDepthStencil(DepthStencilPreset::Type preset);
				
			protected:
				friend class State;

				PipelineState createPipeline(const State::Key *pKey);
				PipelineState getPipeline(const std::string &key);

				Device m_device;

				InputLayoutPool		m_inputLayoutPool;
				
				std::unordered_map<std::string, PipelineState> m_pipelines;
				Blend			m_blends[BlendPreset::NUM];
				Rasterizer		m_rasterizers[RasterizerPreset::NUM];
				DepthStencil	m_depthStencils[DepthStencilPreset::NUM];

				std::function<Blend()>			m_pfnBlends[BlendPreset::NUM];
				std::function<Rasterizer()>		m_pfnRasterizers[RasterizerPreset::NUM];
				std::function<DepthStencil()>	m_pfnDepthStencils[DepthStencilPreset::NUM];
			};
		}
	}
}
