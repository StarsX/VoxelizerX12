//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#pragma once

#include "Voxelizer.h"

using namespace DirectX;

// Note that while ComPtr is used to manage the lifetime of resources on the CPU,
// it has no understanding of the lifetime of resources on the GPU. Apps must account
// for the GPU lifetime of resources to avoid destroying objects that may still be
// referenced by the GPU.
// An example of this can be found in the class method: OnDestroy().
using Microsoft::WRL::ComPtr;

class VoxelizerX : public DXFramework
{
public:
	VoxelizerX(uint32_t width, uint32_t height, std::wstring name);

	virtual void OnInit();
	virtual void OnUpdate();
	virtual void OnRender();
	virtual void OnDestroy();

private:
	static const uint32_t FrameCount = 2;
	static const uint32_t TextureWidth = 1024;
	static const uint32_t TextureHeight = 1024;
	static const uint32_t TexturePixelSize = 4;	// The number of bytes used to represent a pixel in the texture.

	struct Vertex
	{
		XMFLOAT3 position;
		XMFLOAT2 uv;
	};

	XUSG::Shader::Pool				m_shaderPool;
	XUSG::Graphics::Pipeline::Pool	m_pipelinePool;
	XUSG::DescriptorTablePool		m_descriptorTablePool;

	XUSG::PipelineLayout			m_pipelineLayout;
	XUSG::InputLayout 				m_inputLayout;

	std::unique_ptr<Voxelizer>		m_voxelizer;
	DirectX::XMFLOAT4X4				m_proj;

	// Pipeline objects.
	CD3DX12_VIEWPORT	m_viewport;
	CD3DX12_RECT		m_scissorRect;

	ComPtr<IDXGISwapChain3>			m_swapChain;
	ComPtr<ID3D12CommandAllocator>	m_commandAllocator;
	ComPtr<ID3D12CommandQueue>		m_commandQueue;

	XUSG::DescriptorPool m_rtvPool;

	XUSG::Device m_device;
	XUSG::Resource m_renderTargets[FrameCount];
	XUSG::Graphics::PipelineState m_pipelineState;
	XUSG::GraphicsCommandList m_commandList;
	
	XUSG::RenderTargetTable	m_rtvTables[FrameCount];
	XUSG::Descriptor		m_dsv;
	XUSG::DescriptorTable	m_cbvTable;
	XUSG::DescriptorTable	m_srvTable;
	XUSG::DescriptorTable	m_samplerTable;

	// App resources.
	std::unique_ptr<XUSG::DepthStencil>	m_depth;
	std::unique_ptr<XUSG::VertexBuffer>	m_vertexBuffer;
	std::unique_ptr<XUSG::IndexBuffer>	m_indexBuffer;
	std::unique_ptr<XUSG::ConstantBuffer> m_constantBuffer;
	std::unique_ptr<XUSG::Texture2D>	m_textures[8];
	XMFLOAT4 m_cbData_Offset;

	// Synchronization objects.
	uint32_t m_frameIndex;
	HANDLE m_fenceEvent;
	ComPtr<ID3D12Fence> m_fence;
	uint64_t m_fenceValue;

	void LoadPipeline();
	void LoadAssets();
	std::vector<uint8_t> GenerateTextureData(uint32_t subDivLevel);
	void PopulateCommandList();
	void WaitForPreviousFrame();
};
