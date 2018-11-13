//--------------------------------------------------------------------------------------
// By Stars XU Tianchen
//--------------------------------------------------------------------------------------

#pragma once

#include "XUSGType.h"

namespace XUSG
{
	struct SamplerPreset
	{
		enum Type : uint8_t
		{
			POINT_WRAP,
			POINT_CLAMP,
			POINT_BORDER,
			POINT_LESS_EQUAL,

			LINEAR_WRAP,
			LINEAR_CLAMP,
			LINEAR_BORDER,
			LINEAR_LESS_EQUAL,

			ANISOTROPIC_WRAP,
			ANISOTROPIC_CLAMP,
			ANISOTROPIC_BORDER,
			ANISOTROPIC_LESS_EQUAL,

			NUM
		};
	};
	
	class DescriptorTablePool;

	namespace Util
	{
		class DescriptorTable
		{
		public:
			DescriptorTable();
			virtual ~DescriptorTable();

			void SetDescriptors(uint32_t start, uint32_t num, const Descriptor *srcDescriptors);
			void SetSamplers(uint32_t start, uint32_t num, const SamplerPreset::Type *presets,
				DescriptorTablePool &descriptorTablePool);

			XUSG::DescriptorTable CreateCbvSrvUavTable(DescriptorTablePool &descriptorTablePool);
			XUSG::DescriptorTable GetCbvSrvUavTable(DescriptorTablePool &descriptorTablePool);

			XUSG::DescriptorTable CreateSamplerTable(DescriptorTablePool &descriptorTablePool);
			XUSG::DescriptorTable GetSamplerTable(DescriptorTablePool &descriptorTablePool);

			XUSG::RenderTargetTable CreateRtvTable(DescriptorTablePool &descriptorTablePool);
			XUSG::RenderTargetTable GetRtvTable(DescriptorTablePool &descriptorTablePool);

		protected:
			friend class DescriptorTablePool;

			std::string m_key;
		};
	}

	class DescriptorTablePool
	{
	public:
		DescriptorTablePool();
		DescriptorTablePool(const Device &device);
		virtual ~DescriptorTablePool();

		void SetDevice(const Device &device);

		void AllocateCbvSrvUavPool(uint32_t numDescriptors);
		void AllocateSamplerPool(uint32_t numDescriptors);
		void AllocateRtvPool(uint32_t numDescriptors);

		DescriptorTable CreateCbvSrvUavTable(const Util::DescriptorTable &util);
		DescriptorTable GetCbvSrvUavTable(const Util::DescriptorTable &util);

		DescriptorTable CreateSamplerTable(const Util::DescriptorTable &util);
		DescriptorTable GetSamplerTable(const Util::DescriptorTable &util);

		RenderTargetTable CreateRtvTable(const Util::DescriptorTable &util);
		RenderTargetTable GetRtvTable(const Util::DescriptorTable &util);

		const DescriptorPool &GetCbvSrvUavPool() const;
		const DescriptorPool &GetSamplerPool() const;

		const std::shared_ptr<Sampler> &GetSampler(SamplerPreset::Type preset);

	protected:
		friend class Util::DescriptorTable;

		bool allocateCbvSrvUavPool(uint32_t numDescriptors);
		bool allocateSamplerPool(uint32_t numDescriptors);
		bool allocateRtvPool(uint32_t numDescriptors);
		
		bool reallocateCbvSrvUavPool(const std::string &key);
		bool reallocateSamplerPool(const std::string &key);
		bool reallocateRtvPool(const std::string &key);
		
		DescriptorTable createCbvSrvUavTable(const std::string &key);
		DescriptorTable getCbvSrvUavTable(const std::string &key);

		DescriptorTable createSamplerTable(const std::string &key);
		DescriptorTable getSamplerTable(const std::string &key);

		RenderTargetTable createRtvTable(const std::string &key);
		RenderTargetTable getRtvTable(const std::string &key);

		Device m_device;

		std::unordered_map<std::string, DescriptorTable> m_cbvSrvUavTables;
		std::unordered_map<std::string, DescriptorTable> m_samplerTables;
		std::unordered_map<std::string, RenderTargetTable> m_rtvTables;

		DescriptorPool	m_cbvSrvUavPool;
		DescriptorPool	m_samplerPool;
		DescriptorPool	m_rtvPool;

		uint32_t		m_strideCbvSrvUav;
		uint32_t		m_numCbvSrvUavs;

		uint32_t		m_strideSampler;
		uint32_t		m_numSamplers;

		uint32_t		m_strideRtv;
		uint32_t		m_numRtvs;

		std::shared_ptr<Sampler> m_samplerPresets[SamplerPreset::NUM];
		std::function<Sampler()> m_pfnSamplers[SamplerPreset::NUM];
	};
}
