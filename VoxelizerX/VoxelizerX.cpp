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

#include "VoxelizerX.h"
#include "Advanced/XUSGSharedConst.h"

using namespace std;
using namespace XUSG;

enum VertexShader
{
	VS_TRIANGLE,

	NUM_VS
};

enum PixelShader
{
	PS_TRIANGLE,

	NUM_PS
};

VoxelizerX::VoxelizerX(uint32_t width, uint32_t height, std::wstring name) :
	DXFramework(width, height, name),
	m_frameIndex(0),
	m_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
	m_scissorRect(0, 0, static_cast<long>(width), static_cast<long>(height))//,
	//m_rtvDescriptorSize(0)
{
}

void VoxelizerX::OnInit()
{
	LoadPipeline();
	LoadAssets();
}

// Load the rendering pipeline dependencies.
void VoxelizerX::LoadPipeline()
{
	UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
	// Enable the debug layer (requires the Graphics Tools "optional feature").
	// NOTE: Enabling the debug layer after device creation will invalidate the active device.
	{
		ComPtr<ID3D12Debug> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		{
			debugController->EnableDebugLayer();

			// Enable additional debug layers.
			dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
		}
	}
#endif

	ComPtr<IDXGIFactory4> factory;
	ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

	if (m_useWarpDevice)
	{
		ComPtr<IDXGIAdapter> warpAdapter;
		ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));

		ThrowIfFailed(D3D12CreateDevice(
			warpAdapter.Get(),
			D3D_FEATURE_LEVEL_11_0,
			IID_PPV_ARGS(&m_device)
			));
	}
	else
	{
		ComPtr<IDXGIAdapter1> hardwareAdapter;
		GetHardwareAdapter(factory.Get(), &hardwareAdapter);

		ThrowIfFailed(D3D12CreateDevice(
			hardwareAdapter.Get(),
			D3D_FEATURE_LEVEL_11_0,
			IID_PPV_ARGS(&m_device)
			));
	}

	// Describe and create the command queue.
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

	// Describe and create the swap chain.
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.BufferCount = FrameCount;
	swapChainDesc.Width = m_width;
	swapChainDesc.Height = m_height;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.SampleDesc.Count = 1;

	ComPtr<IDXGISwapChain1> swapChain;
	ThrowIfFailed(factory->CreateSwapChainForHwnd(
		m_commandQueue.Get(),		// Swap chain needs the queue so that it can force a flush on it.
		Win32Application::GetHwnd(),
		&swapChainDesc,
		nullptr,
		nullptr,
		&swapChain
		));

	// This sample does not support fullscreen transitions.
	ThrowIfFailed(factory->MakeWindowAssociation(Win32Application::GetHwnd(), DXGI_MWA_NO_ALT_ENTER));

	ThrowIfFailed(swapChain.As(&m_swapChain));
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

	m_descriptorTablePool.SetDevice(m_device);

	// Create frame resources.
	{
		// Create a RTV for each frame.
		DescriptorView rtvs[FrameCount];
		for (UINT n = 0; n < FrameCount; n++)
		{
			ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));

			rtvs[n] = make_shared<Descriptor>();
			rtvs[n]->ResourceType = Descriptor::RTV;
			rtvs[n]->pResource = m_renderTargets[n];
			
			Util::DescriptorTable rtvTable;
			rtvTable.SetDescriptors(0, 1, &rtvs[n]);
			m_swapChainRtvTables[n] = rtvTable.GetRenderTargetTable(m_descriptorTablePool);
		}

		// Create a DSV
		{
			m_depth = make_unique<DepthStencil>(m_device);
			m_depth->SetDescriptorTablePool(m_descriptorTablePool);
			m_depth->Create(m_width, m_height, DXGI_FORMAT_D24_UNORM_S8_UINT, D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE);
			m_dsvHandle = m_depth->GetDsvHandle();
		}
	}

	ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));
}

// Load the sample assets.
void VoxelizerX::LoadAssets()
{
	m_pipelinePool.SetDevice(m_device);
	
	// Create the root signature.
	{
		Util::PipelineLayout pipelineLayout;
		pipelineLayout.SetRange(0, Descriptor::CBV, 1, 0);
		pipelineLayout.SetRange(1, Descriptor::SRV, _countof(m_textures), 0);
		pipelineLayout.SetRange(2, Descriptor::SAMPLER, 1, 0);
		pipelineLayout.SetShaderStage(0, Shader::Stage::VS);
		pipelineLayout.SetShaderStage(1, Shader::Stage::PS);
		pipelineLayout.SetShaderStage(2, Shader::Stage::PS);
		m_pipelineLayout = pipelineLayout.GetPipelineLayout(m_pipelinePool, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	}

	// Create the pipeline state, which includes compiling and loading shaders.
	{
		m_shaderPool.CreateShader(Shader::Stage::VS, VS_TRIANGLE, L"VertexShader.cso");
		m_shaderPool.CreateShader(Shader::Stage::PS, PS_TRIANGLE, L"PixelShader.cso");

		// Define the vertex input layout.
		InputElementTable inputElementDescs =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		m_inputLayout = m_pipelinePool.CreateInputLayout(inputElementDescs);

		// Describe and create the graphics pipeline state object (PSO).
		Graphics::State state;
		state.IASetInputLayout(m_inputLayout);
		state.SetPipelineLayout(m_pipelineLayout);
		state.SetShader(Shader::Stage::VS, m_shaderPool.GetShader(Shader::Stage::VS, VS_TRIANGLE));
		state.SetShader(Shader::Stage::PS, m_shaderPool.GetShader(Shader::Stage::PS, PS_TRIANGLE));
		//state.DSSetState(Graphics::DepthStencilPreset::DEPTH_STENCIL_NONE, m_pipelinePool);
		state.IASetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		state.OMSetNumRenderTargets(1);
		state.OMSetRTVFormat(0, DXGI_FORMAT_R8G8B8A8_UNORM);
		state.OMSetDSVFormat(DXGI_FORMAT_D24_UNORM_S8_UINT);
		m_pipelineState = state.GetPipeline(m_pipelinePool);
	}

	// Create the command list.
	ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_commandList)));

	m_voxelizer = make_unique<Voxelizer>(m_device, m_commandList);

	Resource vbUpload, ibUpload;
	m_voxelizer->Init(m_width, m_height, vbUpload, ibUpload);

	// Create the vertex buffer.
	Resource vertexUpload;
	{
		// Define the geometry for a triangle.
		Vertex triangleVertices[] =
		{
			{ { 0.0f, 0.25f * m_aspectRatio, 0.0f }, { 0.5f, 0.0f } },
			{ { 0.25f, -0.25f * m_aspectRatio, 0.0f }, { 1.0f, 1.0f } },
			{ { -0.25f, -0.25f * m_aspectRatio, 0.0f }, { 0.0f, 1.0f } }
		};
		const uint32_t vertexBufferSize = sizeof(triangleVertices);

		m_vertexBuffer = make_unique<VertexBuffer>(m_device);
		m_vertexBuffer->Create(vertexBufferSize, sizeof(Vertex), D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE);
		m_vertexBuffer->Upload(m_commandList, vertexUpload, triangleVertices);
	}

	// Create the index buffer.
	Resource indexUpload;
	{
		// Define the geometry for a triangle.
		uint16_t triangleIndices[] = { 0, 1, 2 };
		const uint32_t indexBufferSize = sizeof(triangleIndices);

		m_indexBuffer = make_unique<IndexBuffer>(m_device);
		m_indexBuffer->Create(indexBufferSize, DXGI_FORMAT_R16_UINT);
		m_indexBuffer->Upload(m_commandList, indexUpload, triangleIndices);
	}

	// Note: ComPtr's are CPU objects but this resource needs to stay in scope until
	// the command list that references it has finished executing on the GPU.
	// We will flush the GPU at the end of this method to ensure the resource is not
	// prematurely destroyed.
	Resource textureUploads[_countof(m_textures)];

	// Create the constant buffer.
	{
		m_constantBuffer = make_unique<ConstantBuffer>(m_device);
		m_constantBuffer->Create(1024 * 64, sizeof(XMFLOAT4));

		Util::DescriptorTable cbvTable;
		cbvTable.SetDescriptors(0, 1, &m_constantBuffer->GetCBV());
		m_cbvTable = cbvTable.GetDescriptorTable(m_descriptorTablePool);
	}

	// Create the textures.
	{
		// Copy data to the intermediate upload heap and then schedule a copy 
		// from the upload heap to the Texture2D.
		vector<DescriptorView> srvs(_countof(m_textures));
		for (auto i = 0; i < _countof(m_textures); ++i)
		{
			const auto texture = GenerateTextureData(i);

			m_textures[i] = make_unique<Texture2D>(m_device);
			m_textures[i]->Create(TextureWidth, TextureHeight, DXGI_FORMAT_R8G8B8A8_UNORM);
			m_textures[i]->Upload(m_commandList, textureUploads[i], texture.data(), TexturePixelSize);
			srvs[i] = m_textures[i]->GetSRV();
		}
		
		Util::DescriptorTable srvTable;
		srvTable.SetDescriptors(0, _countof(m_textures), srvs.data());
		m_srvTable = srvTable.GetDescriptorTable(m_descriptorTablePool);
	}

	// Create the sampler
	{
		Util::DescriptorTable samplerTable;
		const auto samplerAnisoWrap = SamplerPreset::ANISOTROPIC_WRAP;
		samplerTable.SetSamplers(0, 1, &samplerAnisoWrap, m_descriptorTablePool);
		m_samplerTable = samplerTable.GetDescriptorTable(m_descriptorTablePool);
	}
	
	// Close the command list and execute it to begin the initial GPU setup.
	ThrowIfFailed(m_commandList->Close());
	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// Create synchronization objects and wait until assets have been uploaded to the GPU.
	{
		ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
		m_fenceValue = 1;

		// Create an event handle to use for frame synchronization.
		m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (m_fenceEvent == nullptr)
		{
			ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
		}

		// Wait for the command list to execute; we are reusing the same command 
		// list in our main loop but for now, we just want to wait for setup to 
		// complete before continuing.
		WaitForPreviousFrame();
	}

	// Projection
	const auto aspectRatio = m_width / static_cast<float>(m_height);
	const auto proj = XMMatrixPerspectiveFovLH(g_FOVAngleY, aspectRatio, g_zNear, g_zFar);
	XMStoreFloat4x4(&m_proj, proj);
}

// Generate a simple black and white checkerboard texture.
vector<uint8_t> VoxelizerX::GenerateTextureData(uint32_t subDivLevel)
{
	const auto rowPitch = TextureWidth * TexturePixelSize;
	const auto cellPitch = rowPitch >> subDivLevel;			// The width of a cell in the checkboard texture.
	const auto cellHeight = TextureWidth >> subDivLevel;	// The height of a cell in the checkerboard texture.
	const auto textureSize = rowPitch * TextureHeight;

	vector<uint8_t> data(textureSize);
	uint8_t* pData = &data[0];

	for (auto n = 0u; n < textureSize; n += TexturePixelSize)
	{
		auto x = n % rowPitch;
		auto y = n / rowPitch;
		auto i = x / cellPitch;
		auto j = y / cellHeight;

		if (i % 2 == j % 2)
		{
			pData[n] = 0x00;		// R
			pData[n + 1] = 0x00;	// G
			pData[n + 2] = 0x00;	// B
			pData[n + 3] = 0xff;	// A
		}
		else
		{
			pData[n] = 0xff;		// R
			pData[n + 1] = 0xff;	// G
			pData[n + 2] = 0xff;	// B
			pData[n + 3] = 0xff;	// A
		}
	}

	return data;
}

// Update frame-based values.
void VoxelizerX::OnUpdate()
{
	const float translationSpeed = 0.005f;
	const float offsetBounds = 1.25f;

	m_cbData_Offset.x += translationSpeed;
	if (m_cbData_Offset.x > offsetBounds)
		m_cbData_Offset.x = -offsetBounds;

	// Map and initialize the constant buffer. We don't unmap this until the
		// app closes. Keeping things mapped for the lifetime of the resource is okay.
	const auto pCbData = reinterpret_cast<XMFLOAT4*>(m_constantBuffer->Map());
	*pCbData = m_cbData_Offset;

	// View
	const auto focusPt = XMVectorSet(0.0f, 4.0f, 0.0f, 1.0f);
	const auto eyePt = XMVectorSet(-8.0f, 12.0f, 14.0f, 1.0f);
	const auto view = XMMatrixLookAtLH(eyePt, focusPt, XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f));
	const auto proj = XMLoadFloat4x4(&m_proj);
	m_voxelizer->UpdateFrame(eyePt, view * proj);
}

// Render the scene.
void VoxelizerX::OnRender()
{
	// Record all the commands we need to render the scene into the command list.
	PopulateCommandList();

	// Execute the command list.
	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// Present the frame.
	ThrowIfFailed(m_swapChain->Present(1, 0));

	WaitForPreviousFrame();
}

void VoxelizerX::OnDestroy()
{
	// Ensure that the GPU is no longer referencing resources that are about to be
	// cleaned up by the destructor.
	WaitForPreviousFrame();

	CloseHandle(m_fenceEvent);
}

void VoxelizerX::PopulateCommandList()
{
	// Command list allocators can only be reset when the associated 
	// command lists have finished execution on the GPU; apps should use 
	// fences to determine GPU execution progress.
	ThrowIfFailed(m_commandAllocator->Reset());

	// However, when ExecuteCommandList() is called on a particular command 
	// list, that command list can then be reset at any time and must be before 
	// re-recording.
	ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), nullptr));

	// Set necessary state.
	m_commandList->SetPipelineState(m_pipelineState.Get());
	m_commandList->SetGraphicsRootSignature(m_pipelineLayout.Get());

	DescriptorPool::InterfaceType* ppHeaps[] =
	{
		m_descriptorTablePool.GetSrvUavCbvPool().Get(),
		m_descriptorTablePool.GetSamplerPool().Get()
	};
	m_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	m_commandList->SetGraphicsRootDescriptorTable(0, *m_cbvTable);
	m_commandList->SetGraphicsRootDescriptorTable(1, *m_srvTable);
	m_commandList->SetGraphicsRootDescriptorTable(2, *m_samplerTable);
	m_commandList->RSSetViewports(1, &m_viewport);
	m_commandList->RSSetScissorRects(1, &m_scissorRect);

	// Indicate that the back buffer will be used as a render target.
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	m_commandList->OMSetRenderTargets(1, m_swapChainRtvTables[m_frameIndex].get(), FALSE, m_dsvHandle.get());
	
	// Record commands.
	const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	m_commandList->ClearRenderTargetView(*m_swapChainRtvTables[m_frameIndex], clearColor, 0, nullptr);
	m_commandList->ClearDepthStencilView(*m_dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_commandList->IASetVertexBuffers(0, 1, &m_vertexBuffer->GetVBV());
	m_commandList->IASetIndexBuffer(&m_indexBuffer->GetIBV());
	m_commandList->DrawIndexedInstanced(3, 2, 0, 0, 0);

	m_voxelizer->Render(m_swapChainRtvTables[m_frameIndex], m_dsvHandle);

	// Indicate that the back buffer will now be used to present.
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	ThrowIfFailed(m_commandList->Close());
}

void VoxelizerX::WaitForPreviousFrame()
{
	// WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
	// This is code implemented as such for simplicity. The D3D12HelloFrameBuffering
	// sample illustrates how to use fences for efficient resource usage and to
	// maximize GPU utilization.

	// Signal and increment the fence value.
	const UINT64 fence = m_fenceValue;
	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fence));
	m_fenceValue++;

	// Wait until the previous frame is finished.
	if (m_fence->GetCompletedValue() < fence)
	{
		ThrowIfFailed(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
		WaitForSingleObject(m_fenceEvent, INFINITE);
	}

	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}
