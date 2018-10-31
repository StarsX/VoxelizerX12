//--------------------------------------------------------------------------------------
// By Stars XU Tianchen
//--------------------------------------------------------------------------------------

#pragma once

#include "XUSGType.h"

namespace XUSG
{
	struct Descriptor
	{
		enum Type : uint8_t
		{
			SRV,
			RTV,
			DSV,
			UAV,
			CBV,
			SAMPLER,

			NUM
		};

		Type ResourceType;
		Resource pResource;
		Resource pCounter;
		union
		{
			ShaderResourceView	Srv;
			RenderTargetView	Rtv;
			DepthStencilView	Dsv;
			UnorderedAccessView	Uav;
			ConstantBufferView	Cbv;
			Sampler Samp;
		};
	};
	using DescriptorView = std::shared_ptr<Descriptor>;

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

			void SetDescriptors(uint32_t start, uint32_t num, const DescriptorView *descriptorViews);
			void SetSamplers(uint32_t start, uint32_t num, const SamplerPreset::Type *presets,
				DescriptorTablePool& descriptorTablePool);
			void SetDepthStencil(const DescriptorView& dsv);

			XUSG::DescriptorTable CreateDescriptorTable(DescriptorTablePool& descriptorTablePool);
			XUSG::DescriptorTable GetDescriptorTable(DescriptorTablePool& descriptorTablePool);

			XUSG::RenderTargetTable CreateRenderTargetTable(DescriptorTablePool& descriptorTablePool);
			XUSG::RenderTargetTable GetRenderTargetTable(DescriptorTablePool& descriptorTablePool);

			XUSG::DepthStencilHandle CreateDepthStencilHandle(DescriptorTablePool& descriptorTablePool);
			XUSG::DepthStencilHandle GetDepthStencilHandle(DescriptorTablePool& descriptorTablePool);

		protected:
			friend class DescriptorTablePool;

			std::string m_key;
		};
	}

	class DescriptorTablePool
	{
	public:
		DescriptorTablePool();
		DescriptorTablePool(const Device& device);
		virtual ~DescriptorTablePool();

		void SetDevice(const Device& device);

		void AllocateSrvUavCbvPool(uint32_t numDescriptors);
		void AllocateSamplerPool(uint32_t numDescriptors);
		void AllocateRtvPool(uint32_t numDescriptors);
		void AllocateDsvPool(uint32_t numDescriptors);

		DescriptorTable CreateDescriptorTable(const Util::DescriptorTable& util);
		DescriptorTable GetDescriptorTable(const Util::DescriptorTable& util);

		RenderTargetTable CreateRenderTargetTable(const Util::DescriptorTable& util);
		RenderTargetTable GetRenderTargetTable(const Util::DescriptorTable& util);

		DepthStencilHandle CreateDepthStencilHandle(const Util::DescriptorTable& util);
		DepthStencilHandle GetDepthStencilHandle(const Util::DescriptorTable& util);

		const DescriptorPool& GetSrvUavCbvPool() const;
		const DescriptorPool& GetSamplerPool() const;

		const DescriptorView& GetSampler(SamplerPreset::Type preset);

	protected:
		friend class Util::DescriptorTable;

		void allocateSrvUavCbvPool(uint32_t numDescriptors);
		void allocateSamplerPool(uint32_t numDescriptors);
		void allocateRtvPool(uint32_t numDescriptors);
		void allocateDsvPool(uint32_t numDescriptors);
		void reallocateDescriptorPool(const std::string& key);
		
		DescriptorTable createDescriptorTable(const std::string& key);
		DescriptorTable getDescriptorTable(const std::string& key);

		RenderTargetTable createRenderTargetTable(const std::string& key);
		RenderTargetTable getRenderTargetTable(const std::string& key);

		DepthStencilHandle createDepthStencilHandle(const std::string& key);
		DepthStencilHandle getDepthStencilHandle(const std::string& key);

		std::unordered_map<std::string, DescriptorTable> m_srvUavCbvTables;
		std::unordered_map<std::string, DescriptorTable> m_samplerTables;
		std::unordered_map<std::string, RenderTargetTable> m_rtvTables;
		std::unordered_map<std::string, DepthStencilHandle> m_dsvHandles;

		DescriptorPool	m_srvUavCbvPool;
		DescriptorPool	m_samplerPool;
		DescriptorPool	m_rtvPool;
		DescriptorPool	m_dsvPool;

		uint32_t		m_strideSrvUavCbv;
		uint32_t		m_numSrvUavCbvs;

		uint32_t		m_strideSampler;
		uint32_t		m_numSamplers;

		uint32_t		m_strideRtv;
		uint32_t		m_numRtvs;

		uint32_t		m_strideDsv;
		uint32_t		m_numDsvs;

		DescriptorView	m_samplerPresets[SamplerPreset::NUM];
		std::function<DescriptorView()> m_pfnSamplers[SamplerPreset::NUM];

		Device m_device;
	};
}
