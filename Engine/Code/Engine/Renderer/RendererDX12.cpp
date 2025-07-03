#include "Engine/Renderer/RendererDX12.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Renderer/CommandQueue.hpp"
#include "Engine/Math/IntVec2.hpp"
#include "Engine/Renderer/VertexBufferDX12.hpp"
#include "Engine/Window/Window.hpp"
#include "Engine/Renderer/ShaderDX12.hpp"
#include "Engine/Renderer/RootSignatureDX12.hpp"
#include "Engine/Renderer/PipelineStateObjectDX12.hpp"
#include "Engine/Core/FileUtils.hpp"
#include "Engine/Renderer/DefaultShader.hpp"
#include "Engine/Renderer/ConstantBufferDX12.hpp"
#include "Engine/Renderer/IndexBufferDX12.hpp"
#include "Engine/Renderer/CommandList.hpp"

#include <cassert>

#include <d3dcompiler.h>
#include <dxgi.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dCompiler.lib")

#pragma comment(lib, "dxguid.lib")
#include <dxgidebug.h>
#include <DirectXMath.h>



extern Window* g_window;

RendererDX12::RendererDX12(RendererConfig const& config)
	:Renderer(config)
{
}

void RendererDX12::Startup()
{
	//DX12 CLEANUP: may not be needed
	if (!DirectX::XMVerifyCPUSupport())
	{
		ERROR_AND_DIE("Failed to verify DirectX math library support");
		return;
	}

#if defined(ENGINE_DEBUG_RENDERER)
/*
	m_dxgiDebugModule = (void*)::LoadLibraryA("dxgidebug.dll");
	if (m_dxgiDebugModule == nullptr)
	{
		ERROR_AND_DIE("Could not load dxgidebug.dll");
	}

	typedef HRESULT(WINAPI* GetDebugModuleCB)(REFIID, void**);
	((GetDebugModuleCB)::GetProcAddress((HMODULE)m_dxgiDebugModule, "DXGIGetDebugInterface"))
		(__uuidof(IDXGIDebug), &m_dxgiDebug);

	if (m_dxgiDebug == nullptr)
	{
		ERROR_AND_DIE("Could not load debug module");
	}
*/

	EnableDebugLayer();

#endif
	//Check if tearing is supported 
	m_tearingSupported = CheckTearingSupport();

	//Create Device
	//--------------------------------------------------------------------------------
	IDXGIAdapter4* dxgiAdapter4 = GetAdapter(m_useWarp);
	dxgiAdapter4->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("dxgiAdapter4") + 1, "dxgiAdapter4");
	m_device = CreateDevice(dxgiAdapter4);
	m_device->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("m_device") + 1, "m_device");


	
	//Create Command Queues
	//--------------------------------------------------------------------------------
	m_directCommandQueue = new CommandQueue(this, D3D12_COMMAND_LIST_TYPE_DIRECT);
	m_computeCommandQueue = new CommandQueue(this, D3D12_COMMAND_LIST_TYPE_COMPUTE);
	m_copyCommandQueue = new CommandQueue(this, D3D12_COMMAND_LIST_TYPE_COPY);

	m_directCommandList = m_directCommandQueue->GetCommandList();
	m_copyCommandList = m_copyCommandQueue->GetCommandList();
	m_computeCommandList = m_computeCommandQueue->GetCommandList();
	
	
	//Create Command Queues
	//--------------------------------------------------------------------------------
	for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		m_descriptorAllocators[i] = new DescriptorAllocator((D3D12_DESCRIPTOR_HEAP_TYPE) i, this);
	}

	//Create Swap Chain
	//--------------------------------------------------------------------------------
	IntVec2 clientDims = g_window->GetClientDimensions();
	m_swapChain = CreateSwapChain((HWND)g_window->GetHwnd(), m_directCommandQueue->GetD3D12CommandQueue(),
		(uint32_t)clientDims.x, (uint32_t)clientDims.y, NUM_BUFFER_FRAMES);

	SetSwapChainName(m_swapChain, "m_swapChain");

	//Set current back buffer index
	//--------------------------------------------------------------------------------
	m_currentBackBufferIndex = m_swapChain->GetCurrentBackBufferIndex();

	//Create rtvDescriptor heap and size
	//--------------------------------------------------------------------------------
	m_rtvDescriptorHeap = CreateDescriptorHeap(m_device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, NUM_BUFFER_FRAMES);
	m_rtvDescriptorHeapStartHandle = m_rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	SetDescriptorHeapName(m_rtvDescriptorHeap, "m_rtvDescriptorHeap");

	//Update rtv
	//--------------------------------------------------------------------------------
	UpdateRenderTargetViews(m_device, m_swapChain, m_rtvDescriptorHeap);

	//Create Default Shader
	//--------------------------------------------------------------------------------
	DefaultShader shaderDefault;
	m_defaultShader = CreateShaderDX12("Default", shaderDefault.m_defaultShaderSourceIgnoreTextures); //#TODO:DX12 change this back to default with textures

	/*
	m_cameraCBO = CreateConstantBuffer(sizeof(CameraConstants));
	m_cameraCBO->SetName("m_cameraCBO");
	m_modelCBO = CreateConstantBuffer(sizeof(ModelConstants));
	m_modelCBO->SetName("m_modelCBO");
	m_lightCBO = CreateConstantBuffer(sizeof(LightConstants));
	m_lightCBO->SetName("m_lightCBO");
	m_perFrameCBO = CreateConstantBuffer(sizeof(PerFrameConstants));
	m_perFrameCBO->SetName("m_perFrameCBO");
	*/
	
	m_defaultRootSignature = CreateDefaultRootSignature();
	m_dSVHeap = CreateDescriptorHeapForDepthStencilView();
	IntVec2 windowDims = g_window->GetClientDimensions();
	ResizeDepthBuffer(windowDims.x, windowDims.y);

	DX_SAFE_RELEASE(dxgiAdapter4);
	

}

void RendererDX12::BeginFrame()
{
	if (m_copyCommandList)
	{
		m_copyCommandQueue->ExecuteCommandList(m_copyCommandList);
	}

	if (m_computeCommandList)
	{
		m_computeCommandQueue->ExecuteCommandList(m_computeCommandList);
	}

	if (m_directCommandList)
	{
		m_directCommandQueue->ExecuteCommandList(m_directCommandList);
	}

	m_directCommandList = m_directCommandQueue->GetCommandList();
	m_copyCommandList = m_copyCommandQueue->GetCommandList();
	m_computeCommandList = m_computeCommandQueue->GetCommandList();

	ID3D12Resource* backBuffer = GetCurrentBackBuffer();
	ID3D12GraphicsCommandList2* d3d12CommandList = m_directCommandList->GetGraphicsCommandList();
	D3D12_CPU_DESCRIPTOR_HANDLE dsv = GetCurrentDepthStencilView();
	D3D12_CPU_DESCRIPTOR_HANDLE rtv = GetCurrentRenderTargetView();

	//clear render targets
	{
		TransitionResource(m_directCommandList, backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		ClearScreen(Rgba8::MAGENTA);
		ClearDepthStencilView(dsv);
	}

	d3d12CommandList->OMSetRenderTargets(1, &rtv, false, &dsv);
	m_directCommandList->SetGraphicsRootSignature(*m_defaultRootSignature); //#TODO if I add more root signatures this location will have to change
	m_directCommandList->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST); //This probably will never change
}

void RendererDX12::EndFrame()
{
	m_copyCommandQueue->ExecuteCommandList(m_copyCommandList);
	m_copyCommandList = nullptr;
	m_computeCommandQueue->ExecuteCommandList(m_computeCommandList);
	m_computeCommandList = nullptr;

	ID3D12Resource* backBuffer = GetCurrentBackBuffer();
	TransitionResource(m_directCommandList, backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	m_directCommandQueue->ExecuteCommandList(m_directCommandList);
	Present();

	m_directCommandList = nullptr;
}

unsigned int RendererDX12::Present()
{
	unsigned int syncInterval = m_vSync ? 1 : 0;
	unsigned int presentFlags = m_tearingSupported && !m_vSync ? DXGI_PRESENT_ALLOW_TEARING : 0;
	HRESULT hr = m_swapChain->Present(syncInterval, presentFlags);
	GUARANTEE_OR_DIE(SUCCEEDED(hr), "Could not present from swap chain");
	m_currentBackBufferIndex = m_swapChain->GetCurrentBackBufferIndex();
	return m_currentBackBufferIndex;
}

void RendererDX12::FlushGPU()
{
	m_directCommandQueue->Flush();
	m_computeCommandQueue->Flush();
	m_copyCommandQueue->Flush();
}

void RendererDX12::Shutdown()
{
/*
	delete m_cameraCBO;
	m_cameraCBO = nullptr;

	delete m_modelCBO;
	m_modelCBO = nullptr;

	delete m_lightCBO;
	m_lightCBO = nullptr;

	delete m_perFrameCBO;
	m_perFrameCBO = nullptr;
*/


	//m_directCommandList->Reset();
	//m_copyCommandList->Reset();
	//m_computeCommandList->Reset();
	
	

	for (int i = 0; i < NUM_BUFFER_FRAMES; ++i)
	{
		DX_SAFE_RELEASE(m_backBuffers[i]);
	}



	for (int i = 0; i < (int)m_loadedPipelineStateObjects.size(); ++i)
	{
		delete(m_loadedPipelineStateObjects[i]);
		m_loadedPipelineStateObjects[i] = nullptr;
	}

	for (int i = 0; i < (int)m_loadedShaders.size(); ++i)
	{
		delete(m_loadedShaders[i]);
		m_loadedShaders[i] = nullptr;
	}

	delete m_defaultRootSignature;
	m_defaultRootSignature = nullptr;

	delete m_directCommandList;
	delete m_copyCommandList;
	delete m_computeCommandList;

	DX_SAFE_RELEASE(m_depthBuffer);
	DX_SAFE_RELEASE(m_dSVHeap);
	DX_SAFE_RELEASE(m_rtvDescriptorHeap);
	DX_SAFE_RELEASE(m_swapChain);

	
	delete m_directCommandQueue;
	m_directCommandQueue = nullptr;
	delete m_copyCommandQueue;
	m_copyCommandQueue = nullptr;
	delete m_computeCommandQueue;
	m_computeCommandQueue = nullptr;

// 	m_device->Release();
// 	m_device->Release();

	DX_SAFE_RELEASE(m_device);
	

#if defined(ENGINE_DEBUG_RENDERER)
/*
 	((IDXGIDebug*)m_dxgiDebug)->ReportLiveObjects(
 		DXGI_DEBUG_ALL, (DXGI_DEBUG_RLO_FLAGS)(DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_IGNORE_INTERNAL));

	((IDXGIDebug*)m_dxgiDebug)->Release();
	m_dxgiDebug = nullptr;

	::FreeLibrary((HMODULE)m_dxgiDebugModule);
	m_dxgiDebugModule = nullptr;
*/

	IDXGIDebug1* dxgiDebug =nullptr;
	DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug));
	dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_DETAIL);
	//dxgiDebug->Release();
#endif

}

IDXGIAdapter4* RendererDX12::GetAdapter(bool useWarp)
{
	IDXGIFactory4* dxgiFactory;
	UINT createFactoryFlags = 0;
	HRESULT hr = CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory));
	GUARANTEE_OR_DIE(SUCCEEDED(hr), "could not create D3D12 factory flags");

	IDXGIAdapter1* dxgiAdapter1 = nullptr;
	IDXGIAdapter4* dxgiAdapter4 = nullptr;
	if (useWarp)
	{
		hr = dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&dxgiAdapter1));
		GUARANTEE_OR_DIE(SUCCEEDED(hr), "Could not create warp adapters");
		hr = dxgiAdapter1->QueryInterface(IID_PPV_ARGS(&dxgiAdapter4));
		GUARANTEE_OR_DIE(SUCCEEDED(hr), "Could not query COM object type");
	}

	else
	{
		SIZE_T maxDedicatedVideoMemory = 0;
		for (UINT i = 0; dxgiFactory->EnumAdapters1(i, &dxgiAdapter1) != DXGI_ERROR_NOT_FOUND; ++i)
		{
			DXGI_ADAPTER_DESC1 dxgiAdapterDesc1;
			dxgiAdapter1->GetDesc1(&dxgiAdapterDesc1);

			// Check to see if the adapter can create a D3D12 device without actually 
			// creating it. The adapter with the largest dedicated video memory
			// is favored.
			if ((dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
				SUCCEEDED(D3D12CreateDevice(dxgiAdapter1,
					D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)) &&
				dxgiAdapterDesc1.DedicatedVideoMemory > maxDedicatedVideoMemory)
			{
				maxDedicatedVideoMemory = dxgiAdapterDesc1.DedicatedVideoMemory;
				hr = dxgiAdapter1->QueryInterface(IID_PPV_ARGS(&dxgiAdapter4));
				GUARANTEE_OR_DIE(SUCCEEDED(hr), "Could not query COM object type");
			}
		}
	}

	DX_SAFE_RELEASE(dxgiAdapter1);
	DX_SAFE_RELEASE(dxgiFactory);

	return dxgiAdapter4;
}

ID3D12Device2* RendererDX12::CreateDevice(IDXGIAdapter4* adapter)
{
	//#TODO: Change DX12 device to Device10?
	ID3D12Device2* d3d12Device2 = nullptr;

	//TODO: Change DX12 device to D3D_FEATURE_LEVEL_12_2
	HRESULT hr = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&d3d12Device2));
	GUARANTEE_OR_DIE(SUCCEEDED(hr), "Create D3D12 Device failed");

	//#DX12_RELEASE
	DX_SAFE_RELEASE(adapter);

	// Enable debug messages in debug mode.


#if defined(ENGINE_DEBUG_RENDERER)
	ID3D12InfoQueue* pInfoQueue;
	if (SUCCEEDED(d3d12Device2->QueryInterface(IID_PPV_ARGS(&pInfoQueue))))
	{
		pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
		pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
		//pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

		// Suppress whole categories of messages
 //D3D12_MESSAGE_CATEGORY Categories[] = {};

 // Suppress messages based on their severity level
		D3D12_MESSAGE_SEVERITY Severities[] =
		{
			D3D12_MESSAGE_SEVERITY_INFO
		};

		// Suppress individual messages by their ID
		D3D12_MESSAGE_ID DenyIds[] = {
			D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,   // I'm really not sure how to avoid this message.
			D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,                         // This warning occurs when using capture frame while graphics debugging.
			D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,                       // This warning occurs when using capture frame while graphics debugging.
		};

		D3D12_INFO_QUEUE_FILTER NewFilter = {};
		//NewFilter.DenyList.NumCategories = _countof(Categories);
		//NewFilter.DenyList.pCategoryList = Categories;
		NewFilter.DenyList.NumSeverities = _countof(Severities);
		NewFilter.DenyList.pSeverityList = Severities;
		NewFilter.DenyList.NumIDs = _countof(DenyIds);
		NewFilter.DenyList.pIDList = DenyIds;

		hr = pInfoQueue->PushStorageFilter(&NewFilter);
		GUARANTEE_OR_DIE(SUCCEEDED(hr), "Could not push storage filter");

		DX_SAFE_RELEASE(pInfoQueue);
	}
#endif

	return d3d12Device2;
}

ID3D12Device2* RendererDX12::GetDevice() const
{
	return m_device;
}

unsigned int RendererDX12::GetCurrentBackBufferIndex() const
{
	return m_currentBackBufferIndex;
}

ID3D12Resource* RendererDX12::GetCurrentBackBuffer() const
{
	return m_backBuffers[m_currentBackBufferIndex];
}

D3D12_CPU_DESCRIPTOR_HANDLE RendererDX12::GetCurrentRenderTargetView() const
{
	D3D12_CPU_DESCRIPTOR_HANDLE handle = m_rtvDescriptorHeapStartHandle;
	handle.ptr += m_currentBackBufferIndex * m_rtvDescriptorSize;
	return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE RendererDX12::GetCurrentDepthStencilView() const
{
	return m_dSVHeap->GetCPUDescriptorHandleForHeapStart();
}

DescriptorAllocator* RendererDX12::GetDescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE type) const
{
	return m_descriptorAllocators[(int)type];
}

void RendererDX12::ResizeDepthBuffer(int width, int height)
{
	FlushGPU();
	if (1 > width)
	{
		width = 1;
	}

	if (1 > height)
	{
		height = 1;
	}

	D3D12_CLEAR_VALUE optimizedClearValue = {};
	optimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
	optimizedClearValue.DepthStencil = { 1.0f, 0 };

	D3D12_HEAP_PROPERTIES heapProperties = {};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProperties.CreationNodeMask = 1;
	heapProperties.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC depthDesc = {};
	depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthDesc.Alignment = 0;
	depthDesc.Width = width;
	depthDesc.Height = height;
	depthDesc.DepthOrArraySize = 1;
	depthDesc.MipLevels = 1;
	depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
	depthDesc.SampleDesc.Count = 1;
	depthDesc.SampleDesc.Quality = 0;
	depthDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	HRESULT hr = m_device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&depthDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&optimizedClearValue,
		IID_PPV_ARGS(&m_depthBuffer));

	GUARANTEE_OR_DIE(SUCCEEDED(hr), "Could not create commited Resource");

	// Update the depth-stencil view.
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Texture2D.MipSlice = 0;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

	m_device->CreateDepthStencilView(m_depthBuffer, &dsvDesc,
		m_dSVHeap->GetCPUDescriptorHandleForHeapStart());

	SetResourceName(m_depthBuffer, "m_depthBuffer");
	
}

bool RendererDX12::CheckTearingSupport()
{
	bool allowTearing = false;

	// Rather than create the DXGI 1.5 factory interface directly, we create the
	// DXGI 1.4 interface and query for the 1.5 interface. This is to enable the 
	// graphics debugging tools which will not support the 1.5 factory interface 
	// until a future update.
	IDXGIFactory4* factory4 = nullptr;
	if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory4))))
	{
		IDXGIFactory5* factory5 = nullptr;
		if (SUCCEEDED(factory4->QueryInterface(IID_PPV_ARGS(&factory5))))
		{
			if (FAILED(factory5->CheckFeatureSupport(
				DXGI_FEATURE_PRESENT_ALLOW_TEARING,
				&allowTearing, sizeof(allowTearing))))
			{
				allowTearing = false;
			}
		}
		factory5->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("factory5") + 1, "factory5");
	}

	factory4->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("factory4_2") + 1, "factory4_2");


	return allowTearing == true;
}

IDXGISwapChain4* RendererDX12::CreateSwapChain(HWND hWnd, ID3D12CommandQueue* commandQueue, uint32_t width, uint32_t height, uint32_t bufferCount)
{
	IDXGISwapChain4* dxgiSwapChain4 = nullptr;
	IDXGIFactory4* dxgiFactory4 = nullptr;
	UINT createFactoryFlags = 0;

	HRESULT hr = CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory4));
	GUARANTEE_OR_DIE(SUCCEEDED(hr), "Could not create factory flags");

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.Width = width;
	swapChainDesc.Height = height;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.Stereo = FALSE;
	swapChainDesc.SampleDesc = { 1, 0 };
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = bufferCount;
	swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	// It is recommended to always allow tearing if tearing support is available.
	swapChainDesc.Flags = CheckTearingSupport() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

	IDXGISwapChain1* swapChain1 = nullptr;
	hr = dxgiFactory4->CreateSwapChainForHwnd(
		commandQueue,
		hWnd,
		&swapChainDesc,
		nullptr,
		nullptr,
		&swapChain1);

	GUARANTEE_OR_DIE(SUCCEEDED(hr), "Could not create swap chaing for Hwnd");

	// Disable the Alt+Enter fullscreen toggle feature. Switching to fullscreen
	// will be handled manually.
	hr = dxgiFactory4->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER);
	GUARANTEE_OR_DIE(SUCCEEDED(hr), "Could not make windows assosiaction")

		hr = swapChain1->QueryInterface(IID_PPV_ARGS(&dxgiSwapChain4));
	GUARANTEE_OR_DIE(SUCCEEDED(hr), "Could not query dxgiSwapChain4");

	//#DX12_RELEASE
	DX_SAFE_RELEASE(swapChain1);

	return dxgiSwapChain4;
}

ID3D12DescriptorHeap* RendererDX12::CreateDescriptorHeap(ID3D12Device2* device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors)
{
	ID3D12DescriptorHeap* descriptorHeap = nullptr;

	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = numDescriptors;
	desc.Type = type;

	HRESULT hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptorHeap));
	GUARANTEE_OR_DIE(SUCCEEDED(hr), "could not create descriptor heap");

	return descriptorHeap;
}

void RendererDX12::UpdateRenderTargetViews(ID3D12Device2* device, IDXGISwapChain4* swapChain, ID3D12DescriptorHeap* descriptorHeap)
{
	UINT rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = descriptorHeap->GetCPUDescriptorHandleForHeapStart();

	for (int i = 0; i < NUM_BUFFER_FRAMES; ++i)
	{
		ID3D12Resource* backBuffer = nullptr;
		HRESULT hr = swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer));
		GUARANTEE_OR_DIE(SUCCEEDED(hr), "Could not get swap chain buffer");

		device->CreateRenderTargetView(backBuffer, nullptr, rtvHandle);

		m_backBuffers[i] = backBuffer;
		std::string bufferName = Stringf("m_backBuffer_%i", i);
		m_backBuffers[i]->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen(bufferName.c_str()) + 1, bufferName.c_str());


		//rtvHandle.Offset(rtvDescriptorSize);
		rtvHandle.ptr += rtvDescriptorSize;
	}
}

ID3D12CommandAllocator* RendererDX12::CreateCommandAllocator(ID3D12Device2* device, D3D12_COMMAND_LIST_TYPE type)
{
	ID3D12CommandAllocator* commandAllocator = nullptr;
	HRESULT hr = device->CreateCommandAllocator(type, IID_PPV_ARGS(&commandAllocator));
	GUARANTEE_OR_DIE(SUCCEEDED(hr), "Could not create command allocator");

	std::string typeText;
	switch (type)
	{
	case D3D12_COMMAND_LIST_TYPE_DIRECT:
		typeText = "Direct";
		break;
	case D3D12_COMMAND_LIST_TYPE_BUNDLE:
		typeText = "Bundle";
		break;
	case D3D12_COMMAND_LIST_TYPE_COMPUTE:
		typeText = "Compute";
		break;
	case D3D12_COMMAND_LIST_TYPE_COPY:
		typeText = "Copy";
		break;
	case D3D12_COMMAND_LIST_TYPE_VIDEO_DECODE:
		typeText = "VideoDecode";
		break;
	case D3D12_COMMAND_LIST_TYPE_VIDEO_PROCESS:
		typeText = "VideoProcess";
		break;
	case D3D12_COMMAND_LIST_TYPE_VIDEO_ENCODE:
		typeText = "VideoEncode";
		break;
	default:
		break;
	}

	std::string allocatorName = Stringf("commandAllocator_%s", typeText.c_str());
	SetCommandAllocatorName(commandAllocator, allocatorName);
	return commandAllocator;
}

ID3D12GraphicsCommandList* RendererDX12::CreateCommandList(ID3D12Device2* device, ID3D12CommandAllocator* commandAllocator, D3D12_COMMAND_LIST_TYPE type)
{
	ID3D12GraphicsCommandList* commandList = nullptr;
	HRESULT hr = device->CreateCommandList(0, type, commandAllocator, nullptr, IID_PPV_ARGS(&commandList));
	GUARANTEE_OR_DIE(SUCCEEDED(hr), "Could not create command list");

	hr = commandList->Close();
	GUARANTEE_OR_DIE(SUCCEEDED(hr), "Could not close command list");

	return commandList;
}

ID3D12Fence* RendererDX12::CreateFence(ID3D12Device2* device)
{
	ID3D12Fence* fence = nullptr;
	HRESULT hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	GUARANTEE_OR_DIE(SUCCEEDED(hr), "Could not create Fence");

	return fence;
}

HANDLE RendererDX12::CreateEventHandle()
{
	HANDLE fenceEvent;

	fenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(fenceEvent && "Failed to create fence event.");

	return fenceEvent;
}

void RendererDX12::TransitionResource(CommandList* commandList, ID3D12Resource* resource, D3D12_RESOURCE_STATES beforeState, D3D12_RESOURCE_STATES afterState) const
{
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = resource;
	barrier.Transition.StateBefore = beforeState;
	barrier.Transition.StateAfter = afterState;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	ID3D12GraphicsCommandList2* d3d12CommandList = commandList->GetGraphicsCommandList();
	// Submit the barrier
	d3d12CommandList->ResourceBarrier(1, &barrier);
}

void RendererDX12::ClearRTV(D3D12_CPU_DESCRIPTOR_HANDLE rtv, float* clearColor) const
{
	ID3D12GraphicsCommandList2* d3d12CommandList = m_directCommandList->GetGraphicsCommandList();
	d3d12CommandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
}

void RendererDX12::ClearDepthStencilView( D3D12_CPU_DESCRIPTOR_HANDLE dsv, float depth) const
{
	ID3D12GraphicsCommandList2* d3d12CommandList = m_directCommandList->GetGraphicsCommandList();
	d3d12CommandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, depth, 0, 0, nullptr);
}

std::string RendererDX12::GetPSOName(ShaderDX12* shader, RootSignatureDX12* rootSignature, BlendMode blendMode, DepthMode depthMode, RasterizerMode rasterizerMode) const
{
	std::string shaderName = shader->m_config.m_name;
	std::string rootSigName = rootSignature->GetName();
	std::string blendModeName = GetNameForBlendMode(blendMode);
	std::string depthModeName = GetNameForDepthMode(depthMode);
	std::string rasterizeNodeName = GetNameForRasterizerMode(rasterizerMode);
	return Stringf("PSO: %s-%s-%s-%s-%s", shaderName.c_str(), rootSigName.c_str(), blendModeName.c_str(), depthModeName.c_str(), rasterizeNodeName.c_str());
}

void RendererDX12::EnableDebugLayer()
{
	ID3D12Debug1* debugInterface = nullptr;
	HRESULT hr = D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface));
	GUARANTEE_OR_DIE(SUCCEEDED(hr), "could not get debug interface");
	debugInterface->EnableDebugLayer();
	//debugInterface->SetEnableGPUBasedValidation(TRUE);
}

/*
void RendererDX12::UpdateBufferResource(ID3D12GraphicsCommandList2* commandList, ID3D12Resource** pDestinationResource, ID3D12Resource** pIntermediateResource, size_t numElements, size_t elementSize, const void* bufferData, D3D12_RESOURCE_FLAGS flags)
{
	size_t bufferSize = numElements * elementSize;

	D3D12_HEAP_PROPERTIES heapProperties = {};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProperties.CreationNodeMask = 1;
	heapProperties.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Alignment = 0;
	resourceDesc.Width = bufferSize;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.Flags = flags;

	HRESULT hr = m_device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(pDestinationResource));

	GUARANTEE_OR_DIE(SUCCEEDED(hr), "could not create commited resource");


	heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProperties.CreationNodeMask = 1;
	heapProperties.VisibleNodeMask = 1;


	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Alignment = 0;
	resourceDesc.Width = bufferSize;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.Flags = D3D12_RESOURCE_FLAGS();
	if (bufferData)
	{
		hr = m_device->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(pIntermediateResource));

		GUARANTEE_OR_DIE(SUCCEEDED(hr), "Could not create commited resource");

		D3D12_SUBRESOURCE_DATA subresourceData = {};
		subresourceData.pData = bufferData;
		subresourceData.RowPitch = bufferSize;
		subresourceData.SlicePitch = subresourceData.RowPitch;

		UpdateSubresources(commandList, *pDestinationResource, *pIntermediateResource, 0, 0, 1, &subresourceData);
	}
}
*/


void RendererDX12::ClearScreen(Rgba8 const& clearColor)
{
	float clearColorFloat [4] = {};
	clearColor.GetAsFloats(clearColorFloat);
	ClearRTV(GetCurrentRenderTargetView(), clearColorFloat);
}

void RendererDX12::BeginCamera(Camera const& camera)
{
	Vec2 screenBottomLeft = camera.GetOrthoBottomLeft();
	Vec2 screenTopRight = camera.GetOrthoTopRight();

	Vec2 clientDims((float)g_window->GetClientDimensions().x, (float)g_window->GetClientDimensions().y);
	AABB2 viewportNormalizedBounds = camera.GetViewportBounds();
	float viewportWidth = (viewportNormalizedBounds.m_maxs.x - viewportNormalizedBounds.m_mins.x) * clientDims.x;
	float viewportHeight = (viewportNormalizedBounds.m_maxs.y - viewportNormalizedBounds.m_mins.y) * clientDims.y;

	float topLeftX = viewportNormalizedBounds.m_mins.x * clientDims.x;
	float topLeftY = (1.f - viewportNormalizedBounds.m_maxs.y) * clientDims.y;
	D3D12_VIEWPORT viewport = {};
	viewport.TopLeftX = topLeftX;
	viewport.TopLeftY = topLeftY;
	viewport.Width = viewportWidth;
	viewport.Height = viewportHeight;
	viewport.MinDepth = 0.f;
	viewport.MaxDepth = 1.f;

	m_directCommandList->SetViewport(viewport);

	D3D12_RECT scissorRect = { 0,0, (LONG)viewportWidth, (LONG)viewportHeight };
	m_directCommandList->SetScissorRect(scissorRect);


	CameraConstants cameraConstants;
	cameraConstants.m_worldToCameraTransform = camera.GetWorldToCameraTransform();
	cameraConstants.m_cameraToRenderTransform = camera.GetCameraToRenderTransform();
	cameraConstants.m_renderToClipTransform = camera.GetRenderToClipTransform();
	cameraConstants.m_cameraPosition = camera.GetPosition();
	m_directCommandList->SetGraphicsDynamicConstantBuffer(k_cameraConstantsSlot - 1, cameraConstants);

	ModelConstants modelConstants;
	modelConstants.m_modelToWorldTransform = Mat44::IDENTITY;
	modelConstants.m_modelColor[0] = 1.f;
	modelConstants.m_modelColor[1] = 1.f;
	modelConstants.m_modelColor[2] = 1.f;
	modelConstants.m_modelColor[3] = 1.f;
	m_directCommandList->SetGraphicsDynamicConstantBuffer(k_modelConstantsSlot - 1, modelConstants);
}

void RendererDX12::EndCamera(Camera const& camera)
{
	UNUSED(camera);
}

void RendererDX12::BeginRendererEvent(char const* eventName)
{
}

void RendererDX12::EndRendererEvent()
{
}

void RendererDX12::BindPSO(PipelineStateObjectDX12* pipelineStateObject)
{
	m_directCommandList->SetPipelineState(pipelineStateObject->m_pipelineState);
}

void RendererDX12::BindRootSignature(RootSignatureDX12* rootSignature)
{
	if (!rootSignature)
	{
		m_directCommandList->SetGraphicsRootSignature(*m_defaultRootSignature);
		return;
	}

	m_directCommandList->SetGraphicsRootSignature(*rootSignature);
}

void RendererDX12::DrawVertexArray(Verts const& verts)
{
	m_directCommandList->SetDynamicVertexBuffer(0, verts);
	m_directCommandList->Draw((uint32_t)verts.size(), 1, 0, 0);
}

void RendererDX12::DrawVertexArray(VertTBNs const& verts)
{
	m_directCommandList->SetDynamicVertexBuffer(0, verts);
	m_directCommandList->Draw((uint32_t)verts.size(), 1, 0, 0);
}

void RendererDX12::DrawVertexBuffer(VertexBufferDX12* vbo, unsigned int vertexCount)
{
	m_directCommandList->SetVertexBuffer(0, *vbo);
	m_directCommandList->Draw(vertexCount, 1, 0, 0);
}

void RendererDX12::DrawIndexedVertexBuffer(VertexBufferDX12* vbo, IndexBufferDX12* ibo, unsigned int indexCount)
{
	m_directCommandList->SetVertexBuffer(0, *vbo);
	m_directCommandList->SetIndexBuffer(*ibo);
	m_directCommandList->DrawIndexed(indexCount, 1, 0, 0, 0);
}

DescriptorAllocation RendererDX12::AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE type, unsigned int numDescriptors)
{
	return m_descriptorAllocators[type]->Allocate(numDescriptors);
}

Texture* RendererDX12::CreateOrGetTextureFromFile(char const* imageFilePath)
{
	return nullptr;
}

BitmapFont* RendererDX12::CreatOrGetBitMapFontFromFile(char const* bitmapFontFilePathWithNoExtension)
{
	return nullptr;
}

VertexBufferDX12* RendererDX12::CreateVertexBuffer(Verts const& verts, std::string const& name)
{
	VertexBufferDX12* newVertexBuffer = new VertexBufferDX12(this, name);
	m_copyCommandList->CopyVertexBuffer( *newVertexBuffer, verts);
	return newVertexBuffer;
}

VertexBufferDX12* RendererDX12::CreateVertexBuffer(VertTBNs const& verts, std::string const& name)
{
	VertexBufferDX12* newVertexBuffer = new VertexBufferDX12(this, name);
	m_copyCommandList->CopyVertexBuffer(*newVertexBuffer, verts);
	return newVertexBuffer;
}

IndexBufferDX12* RendererDX12::CreateIndexBuffer(std::vector<unsigned int> indexes, std::string const& name)
{
	IndexBufferDX12* newIndexBuffer = new IndexBufferDX12(this, name);
	m_copyCommandList->CopyIndexBuffer(*newIndexBuffer, indexes);
	return newIndexBuffer;
}

ShaderDX12* RendererDX12::CreateOrGetShaderDX12FromFile(char const* shaderName, VertexType vertexType)
{
	for (int shaderIndex = 0; shaderIndex < (int)m_loadedShaders.size(); ++shaderIndex)
	{
		if (m_loadedShaders[shaderIndex]->GetName() == (std::string)shaderName)
		{
			return m_loadedShaders[shaderIndex];
		}
	}
	std::string shaderSource;
	std::string shaderString = (std::string)shaderName;
	shaderString.append(".hlsl");
	FileReadToString(shaderSource, shaderString);
	return CreateShaderDX12(shaderName, shaderSource.c_str(), vertexType);
}

ShaderDX12* RendererDX12::CreateShaderDX12(char const* shaderName, char const* shaderSource, VertexType vertexType)
{
	ShaderConfig shaderConfig;
	shaderConfig.m_name = shaderName;

	DWORD shaderFlags = D3DCOMPILE_OPTIMIZATION_LEVEL3;
#if defined(ENGINE_DEBUG_RENDERER)
	shaderFlags = D3DCOMPILE_DEBUG;
	shaderFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
	shaderFlags |= D3DCOMPILE_WARNINGS_ARE_ERRORS;
#endif

	ShaderDX12* newShader = new ShaderDX12(shaderConfig);
	ID3DBlob* errorBlob = nullptr;

	//Compile Vertex Shader
	HRESULT hr = D3DCompile(shaderSource, strlen(shaderSource), "VertexShader", nullptr, nullptr, "VertexMain", "vs_5_0", shaderFlags, 0, &newShader->m_vertexShaderBlob, &errorBlob);
	if (!SUCCEEDED(hr))
	{
		if (errorBlob != NULL)
		{
			DebuggerPrintf((char*)(errorBlob->GetBufferPointer()));
		}
		ERROR_AND_DIE("Could not compile vertex shader");
	}


	if (errorBlob != NULL)
	{
		errorBlob->Release();
	}

	//Compile Pixel Shader
	hr = D3DCompile(shaderSource, strlen(shaderSource), "PixelShader", nullptr, nullptr, "PixelMain", "ps_5_0", shaderFlags, 0, &newShader->m_pixelShaderBlob, &errorBlob);
	if (!SUCCEEDED(hr))
	{
		if (errorBlob != NULL)
		{
			DebuggerPrintf((char*)(errorBlob->GetBufferPointer()));
		}
		ERROR_AND_DIE("Could not compile pixel shader");
	}

	if (errorBlob != NULL)
	{
		errorBlob->Release();
	}

	//Set Shader vertex type which will be used to make input layout inside CreatePSO()
	newShader->m_vertexType = vertexType;

	m_loadedShaders.push_back(newShader);
	return newShader;
}

ShaderDX12* RendererDX12::CreateShaderDX12(char const* shaderName, VertexType vertexType)
{
	std::string shaderSource = Stringf("Data/Shaders/%s.hlsl", shaderName);
	std::string outString;
	FileReadToString(outString, shaderSource);
	return CreateShaderDX12(shaderName, outString.c_str(), vertexType);
}


ConstantBufferDX12* RendererDX12::CreateConstantBuffer(const unsigned int size)
{
	ConstantBufferDX12* newCBO = new ConstantBufferDX12(this);
	newCBO->CreateViews(1, size);
	return new ConstantBufferDX12(this);
}

PipelineStateObjectDX12* RendererDX12::CreateOrGetPSO(ShaderDX12* shader, RootSignatureDX12* rootSignature, BlendMode blendMode, DepthMode depthMode, RasterizerMode rasterizerMode)
{
	RootSignatureDX12* usedRootSignature = rootSignature;
	if (!usedRootSignature)
	{
		usedRootSignature = m_defaultRootSignature;
	}

	ShaderDX12* usedShader = shader;
	if (!usedShader)
	{
		usedShader = m_defaultShader;
	}

	std::string psoName = GetPSOName(usedShader, usedRootSignature, blendMode, depthMode, rasterizerMode);
	for (int i = 0; i < (int)m_loadedPipelineStateObjects.size(); ++i)
	{
		if (m_loadedPipelineStateObjects[i]->GetName() == psoName)
		{
			return m_loadedPipelineStateObjects[i];
		}
	}

	return CreatePSO(usedShader, usedRootSignature, blendMode, depthMode, rasterizerMode);
}


RootSignatureDX12* RendererDX12::CreateDefaultRootSignature()
{
	//#TODO: DX12 this root signature is set up for default shader, instead need to make it work with any shader

	D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
	featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	HRESULT hr = m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData));

	if (!SUCCEEDED(hr))
	{
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}

	// Allow input layout and deny unnecessary access to certain pipeline stages.
	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	//Root Parameter 0 : PerFrameConstants(b1)
	D3D12_ROOT_PARAMETER1 perFrameCBV = {};
	perFrameCBV.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	perFrameCBV.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	perFrameCBV.Descriptor.ShaderRegister = k_perFrameConstantsSlot; // b1
	perFrameCBV.Descriptor.RegisterSpace = 0;
	perFrameCBV.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;

	//Root Parameter 1: CameraConstants(b2)
	D3D12_ROOT_PARAMETER1 cameraCBV = {};
	cameraCBV.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	cameraCBV.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	cameraCBV.Descriptor.ShaderRegister = k_cameraConstantsSlot; // b2
	cameraCBV.Descriptor.RegisterSpace = 0;
	cameraCBV.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;

	//Root Parameter 2 : ModelConstants(b3)
	D3D12_ROOT_PARAMETER1 modelCBV = {};
	modelCBV.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	modelCBV.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	modelCBV.Descriptor.ShaderRegister = k_modelConstantsSlot; // b3
	modelCBV.Descriptor.RegisterSpace = 0;
	modelCBV.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;

	//Root Parameter 3 : LightConstants(b4)
	D3D12_ROOT_PARAMETER1 lightsCBV = {};
	lightsCBV.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	lightsCBV.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	lightsCBV.Descriptor.ShaderRegister = k_lightConstantsSlot; // b1
	lightsCBV.Descriptor.RegisterSpace = 0;
	lightsCBV.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;

	//Root Parameter 4 : Descriptor Table for Texture (t0)
	D3D12_DESCRIPTOR_RANGE1 srvRange = {};
	srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	srvRange.NumDescriptors = 1;
	srvRange.BaseShaderRegister = 0; // t0
	srvRange.RegisterSpace = 0;
	srvRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
	srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_DESCRIPTOR_TABLE1 srvTable = {};
	srvTable.NumDescriptorRanges = 1;
	srvTable.pDescriptorRanges = &srvRange;

	D3D12_ROOT_PARAMETER1 textureParam = {};
	textureParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	textureParam.DescriptorTable = srvTable;
	textureParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// --- Root Parameter 5: Descriptor Table for Sampler (s0) ---
	D3D12_DESCRIPTOR_RANGE1 sampRange = {};
	sampRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
	sampRange.NumDescriptors = 1;
	sampRange.BaseShaderRegister = 0; // s0
	sampRange.RegisterSpace = 0;
	sampRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
	sampRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_DESCRIPTOR_TABLE1 sampTable = {};
	sampTable.NumDescriptorRanges = 1;
	sampTable.pDescriptorRanges = &sampRange;

	D3D12_ROOT_PARAMETER1 samplerParam = {};
	samplerParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	samplerParam.DescriptorTable = sampTable;
	samplerParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	// --- Combine all root parameters ---
	D3D12_ROOT_PARAMETER1 rootParams[6] = {
		perFrameCBV,
		cameraCBV,
		modelCBV,
		lightsCBV,
		textureParam,
		samplerParam
	};

	D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
	rootSignatureDesc.NumParameters = _countof(rootParams);
	rootSignatureDesc.pParameters = rootParams;
	rootSignatureDesc.NumStaticSamplers = 0;
	rootSignatureDesc.Flags = rootSignatureFlags;

	RootSignatureDX12* newRootSignature = new RootSignatureDX12(this, rootSignatureDesc, featureData.HighestVersion, "RootSignature_Default");
	return newRootSignature;
}

PipelineStateObjectDX12* RendererDX12::CreatePSO(ShaderDX12* shader, RootSignatureDX12* rootSignature, BlendMode blendMode, DepthMode depthMode, RasterizerMode rasterizerMode)
{
	std::string psoName = GetPSOName(shader, rootSignature, blendMode, depthMode, rasterizerMode);
	PipelineStateObjectDX12* newPSO = new PipelineStateObjectDX12(shader, rootSignature, psoName);

	//#TODO: DX12 figure out if rtvFormats will always be the same
	D3D12_RT_FORMAT_ARRAY rtvFormats = {};
	rtvFormats.NumRenderTargets = 1;
	rtvFormats.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

	//Create Input Layout
	//----------------------------------------------------------------------------------------
	D3D12_INPUT_ELEMENT_DESC inputLayout_VertsPCU[] = {
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	D3D12_INPUT_ELEMENT_DESC inputLayout_VertsPCUTBN[] = {
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
	{ "BITANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
	{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
	};

	D3D12_INPUT_ELEMENT_DESC* usedInputLayout = shader->m_vertexType == VertexType::VERTEX_PCU ? inputLayout_VertsPCU : inputLayout_VertsPCUTBN;
	int numInputElements = shader->m_vertexType == VertexType::VERTEX_PCU ? _countof(inputLayout_VertsPCU) : _countof(inputLayout_VertsPCUTBN);

	//Create Blend State description
	//----------------------------------------------------------------------------------------
	D3D12_BLEND_DESC blendDesc = {};
	blendDesc.AlphaToCoverageEnable = false;
	blendDesc.IndependentBlendEnable = false;

	D3D12_RENDER_TARGET_BLEND_DESC& rtBlend = blendDesc.RenderTarget[0];
	rtBlend.BlendEnable = true;
	rtBlend.LogicOpEnable = false;
	rtBlend.BlendOp = D3D12_BLEND_OP_ADD;
	rtBlend.SrcBlendAlpha = D3D12_BLEND_ONE;
	rtBlend.DestBlendAlpha = D3D12_BLEND_ZERO;
	rtBlend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	rtBlend.LogicOp = D3D12_LOGIC_OP_NOOP;
	rtBlend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	switch (blendMode)
	{
	case BlendMode::OPAQUE:
		rtBlend.SrcBlend = D3D12_BLEND_ONE;
		rtBlend.DestBlend = D3D12_BLEND_ZERO;
		break;
	case BlendMode::ALPHA:
		rtBlend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
		rtBlend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		break;
	case BlendMode::ADDITIVE:
		rtBlend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
		rtBlend.DestBlend = D3D12_BLEND_ZERO;
		break;
	case BlendMode::LIGHTEN:
		rtBlend.SrcBlend = D3D12_BLEND_ONE;
		rtBlend.DestBlend = D3D12_BLEND_ONE;
		rtBlend.BlendOp = D3D12_BLEND_OP_MAX;
		break;
	default:
		rtBlend.SrcBlend = D3D12_BLEND_ONE;
		rtBlend.DestBlend = D3D12_BLEND_ZERO;
		break;
	}

	//Create DepthStencil description
	//----------------------------------------------------------------------------------------
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
	depthStencilDesc.StencilEnable = true;
	depthStencilDesc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;  // usually 0xFF
	depthStencilDesc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK; // usually 0xFF
	depthStencilDesc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	depthStencilDesc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	switch (depthMode)
	{
	case DepthMode::DISABLED:
		depthStencilDesc.DepthEnable = false;
		depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		break;
	case DepthMode::READ_ONLY_ALWAYS:
		depthStencilDesc.DepthEnable = true;
		depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		break;
	case DepthMode::READ_ONLY_LESS_EQUAL:
		depthStencilDesc.DepthEnable = true;
		depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		break;
	case DepthMode::READ_WRITE_LESS_EQUAL:
		depthStencilDesc.DepthEnable = true;
		depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		break;
	default:
		depthStencilDesc.DepthEnable = true;
		depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		break;
	}

	//Create rasterizer description
	//----------------------------------------------------------------------------------------
	D3D12_RASTERIZER_DESC rasterDesc = {};
	rasterDesc.FrontCounterClockwise = false;
	rasterDesc.DepthClipEnable = true;

	switch (rasterizerMode)
	{
	case RasterizerMode::SOLID_CULL_NONE:
		rasterDesc.FillMode = D3D12_FILL_MODE_SOLID;
		rasterDesc.CullMode = D3D12_CULL_MODE_NONE;
		break;
	case RasterizerMode::SOLID_CULL_BACK:
		rasterDesc.FillMode = D3D12_FILL_MODE_SOLID;
		rasterDesc.CullMode = D3D12_CULL_MODE_BACK;
		break;
	case RasterizerMode::WIREFRAME_CULL_NONE:
		rasterDesc.FillMode = D3D12_FILL_MODE_WIREFRAME;
		rasterDesc.CullMode = D3D12_CULL_MODE_NONE;
		break;
	case RasterizerMode::WIREFRAME_CULL_BACK:
		rasterDesc.FillMode = D3D12_FILL_MODE_WIREFRAME;
		rasterDesc.CullMode = D3D12_CULL_MODE_BACK;
		break;
	case RasterizerMode::WIREFRAME_CULL_FRONT:
		rasterDesc.FillMode = D3D12_FILL_MODE_WIREFRAME;
		rasterDesc.CullMode = D3D12_CULL_MODE_FRONT;
		break;
	default:
		rasterDesc.FillMode = D3D12_FILL_MODE_SOLID;
		rasterDesc.CullMode = D3D12_CULL_MODE_NONE;
		break;
	}

	//Create Pipeline state stream
	//----------------------------------------------------------------------------------------
	PipelineStateStream pipelineStateStream = {};
	pipelineStateStream.RootSignature.m_rootSignature = rootSignature->GetRootSignature();
	pipelineStateStream.InputLayoutDesc.m_inputLayoutDesc.pInputElementDescs = usedInputLayout;
	pipelineStateStream.InputLayoutDesc.m_inputLayoutDesc.NumElements = numInputElements;
	pipelineStateStream.PrimitiveTopologyType.m_typologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pipelineStateStream.VertexShader.m_shaderByteCode.pShaderBytecode = shader->m_vertexShaderBlob->GetBufferPointer();
	pipelineStateStream.VertexShader.m_shaderByteCode.BytecodeLength = shader->m_vertexShaderBlob->GetBufferSize();
	pipelineStateStream.PixelShader.m_shaderByteCode.pShaderBytecode = shader->m_pixelShaderBlob->GetBufferPointer();
	pipelineStateStream.PixelShader.m_shaderByteCode.BytecodeLength = shader->m_pixelShaderBlob->GetBufferSize();
	pipelineStateStream.DepthStencilState.m_desc = depthStencilDesc;
	pipelineStateStream.DepthStencilViewFormat.m_format = DXGI_FORMAT_D32_FLOAT;
	pipelineStateStream.RenderTargetViewFormats.m_formats = rtvFormats;
	pipelineStateStream.RasterizerState.m_desc = rasterDesc;
	pipelineStateStream.BlendState.m_desc = blendDesc;

	D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = { sizeof(PipelineStateStream), &pipelineStateStream };

	//Create PSO
	//----------------------------------------------------------------------------------------
	HRESULT hr = m_device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&newPSO->m_pipelineState));
	GUARANTEE_OR_DIE(SUCCEEDED(hr), "could not create pipeline state");

	SetPipelineStateName(newPSO->m_pipelineState, psoName);


	m_loadedPipelineStateObjects.push_back(newPSO);
	return newPSO;
}

ID3D12DescriptorHeap* RendererDX12::CreateDescriptorHeapForDepthStencilView()
{
	ID3D12DescriptorHeap* newDSV = nullptr;

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	HRESULT hr = m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&newDSV));
	GUARANTEE_OR_DIE(SUCCEEDED(hr), "could not create Descriptor Heap");
	return newDSV;
}

void RendererDX12::SetModelConstants(Mat44 const& modelToWorldTransform, Rgba8 const& modelColor)
{
	ModelConstants modelConstants;
	modelConstants.m_modelToWorldTransform = modelToWorldTransform;
	float modelColorAsFloats [4] = {};
	modelColor.GetAsFloats(modelColorAsFloats);
	m_directCommandList->SetGraphicsDynamicConstantBuffer(k_modelConstantsSlot, modelConstants);
}

void RendererDX12::SetLightConstants(Vec3 const& sunDirection, float sunIntensity, float ambientIntensity, Rgba8 const& sunColor)
{

	LightConstants lightConstant;
	lightConstant.m_sunDirection = sunDirection;
	lightConstant.m_sunIntensity = sunIntensity;
	lightConstant.m_ambientIntensity = ambientIntensity;
	lightConstant.m_sunColorRGB[0] = NormalizeByte(sunColor.r);
	lightConstant.m_sunColorRGB[1] = NormalizeByte(sunColor.g);
	lightConstant.m_sunColorRGB[2] = NormalizeByte(sunColor.b);
	m_directCommandList->SetGraphicsDynamicConstantBuffer(k_lightConstantsSlot, lightConstant);
}

void RendererDX12::SetLightConstants(LightConstants const& lightConstants)
{
	m_directCommandList->SetGraphicsDynamicConstantBuffer(k_lightConstantsSlot, lightConstants);
}

void RendererDX12::SetColorAdjustmentConstants(ColorAdjustmentConstants const& colorAdjustmentConstants)
{
	m_directCommandList->SetGraphicsDynamicConstantBuffer(k_colorAdjustmentConstantsSlot, colorAdjustmentConstants);
}

void RendererDX12::SetPerFrameConstants(PerFrameConstants const& perFrameConstants)
{
	m_directCommandList->SetGraphicsDynamicConstantBuffer(k_perFrameConstantsSlot, perFrameConstants);
}


CommandQueue* RendererDX12::GetCommandQueue(D3D12_COMMAND_LIST_TYPE type) const
{

	switch (type)
	{
	case D3D12_COMMAND_LIST_TYPE_DIRECT: return m_directCommandQueue;

	case D3D12_COMMAND_LIST_TYPE_COMPUTE: return m_computeCommandQueue;

	case D3D12_COMMAND_LIST_TYPE_COPY: return m_copyCommandQueue;

	}

	return nullptr;
}

