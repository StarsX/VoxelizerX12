//--------------------------------------------------------------------------------------
// By Stars XU Tianchen
//--------------------------------------------------------------------------------------

#pragma once

namespace XUSG
{
	using Microsoft::WRL::ComPtr;

	// Device and blobs
	using BlobType = ID3DBlob;
	using Blob = ComPtr<BlobType>;
	using Device = ComPtr<ID3D12Device>;

	// Command lists related
	using CommandList = ID3D12CommandList*;
	using GraphicsCommandList = ComPtr<ID3D12GraphicsCommandList>;

	// Resources related
	using Resource = ComPtr<ID3D12Resource>;
	using VertexBufferView = D3D12_VERTEX_BUFFER_VIEW;
	using IndexBufferView = D3D12_INDEX_BUFFER_VIEW;
	using Sampler = D3D12_SAMPLER_DESC;

	using ResourceState = D3D12_RESOURCE_STATES;
	using ResourceBarrier = D3D12_RESOURCE_BARRIER;

	// Descriptors related
	using DescriptorPool = ComPtr<ID3D12DescriptorHeap>;
	using Descriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE;
	using DescriptorView = CD3DX12_GPU_DESCRIPTOR_HANDLE;
	using DescriptorTable = std::shared_ptr<DescriptorView>;
	using RenderTargetTable = std::shared_ptr<Descriptor>;

	// Pipeline layouts related
	using PipelineLayout = ComPtr<ID3D12RootSignature>;
	using DescriptorRangeList = std::vector<CD3DX12_DESCRIPTOR_RANGE1>;

	// Input layouts related
	using InputElementTable = std::vector<D3D12_INPUT_ELEMENT_DESC>;
	struct InputLayoutDesc : D3D12_INPUT_LAYOUT_DESC
	{
		InputElementTable elements;
	};
	using InputLayout = std::shared_ptr<InputLayoutDesc>;

	// Primitive related
	using PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE;
	using PrimitiveTopology = D3D12_PRIMITIVE_TOPOLOGY;
	
	// Format and resources related
	using Format = DXGI_FORMAT;
	using PoolType = D3D12_HEAP_TYPE;
	using ResourceFlags = D3D12_RESOURCE_FLAGS;

	// Pipeline layouts related
	struct RootParameter : CD3DX12_ROOT_PARAMETER1
	{
		DescriptorRangeList ranges;
	};
	using DescriptorTableLayout = std::shared_ptr<RootParameter>;

	// Shaders related
	namespace Shader
	{
		using ByteCode = CD3DX12_SHADER_BYTECODE;
		using Reflector = ComPtr<ID3D12ShaderReflection>;
	}

	// Graphics pipelines related
	namespace Graphics
	{
		using PipelineState = ComPtr<ID3D12PipelineState>;
		using PipelineDesc = D3D12_GRAPHICS_PIPELINE_STATE_DESC;

		using Blend = std::shared_ptr<D3D12_BLEND_DESC>;
		using Rasterizer = std::shared_ptr < D3D12_RASTERIZER_DESC>;
		using DepthStencil = std::shared_ptr < D3D12_DEPTH_STENCIL_DESC>;
	}
}
