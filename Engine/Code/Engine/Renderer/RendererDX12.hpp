#pragma once
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Renderer/DX12Utils.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include <d3d12.h>  
#pragma comment(lib, "d3d12.lib")
#include <dxgi1_6.h>
#include <chrono>
#include <memory>
#undef OPAQUE

class Window;
class CommandQueue;
class VertexBufferDX12;
class IndexBufferDX12;
class ShaderDX12;
class RootSignatureDX12;
class DescriptorAllocator;
class DescriptorAllocation;
class ConstantBufferDX12;
class PipelineStateObjectDX12;
struct IDXGIAdapter4;
struct ID3D12Device2;

class CommandList;
typedef std::shared_ptr<CommandList> SharedCommandList;

constexpr int NUM_BUFFER_FRAMES = 3;

class RendererDX12 : public Renderer
{
public:
	RendererDX12(RendererConfig const& config);
	virtual ~RendererDX12() {};

	virtual void	Startup() override;
	virtual void	BeginFrame() override;
	virtual void	EndFrame() override;
	unsigned int	Present();
	void			FlushGPU();
	virtual void	Shutdown() override;

	virtual void	ClearScreen(Rgba8 const& clearColor) override;
	virtual void	BeginCamera(Camera const& camera) override;
	virtual void	EndCamera(Camera const& camera) override;

	virtual void	BeginRendererEvent(char const* eventName) override;
	virtual void	EndRendererEvent() override;

	void			BindPSO(PipelineStateObjectDX12* pipelineStateObject);
	void			BindRootSignature(RootSignatureDX12* rootSignature);

	void			DrawVertexArray(Verts const& verts);
	void			DrawVertexArray(VertTBNs const& verts);
	void			DrawVertexBuffer(VertexBufferDX12* vbo, unsigned int vertexCount);
	void			DrawIndexedVertexBuffer(VertexBufferDX12* vbo, IndexBufferDX12* ibo, unsigned int indexCount);

	DescriptorAllocation AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE type, unsigned int numDescriptors = 1);


	//Creation
	virtual Texture*	CreateOrGetTextureFromFile(char const* imageFilePath) override;
	virtual BitmapFont* CreatOrGetBitMapFontFromFile(char const* bitmapFontFilePathWithNoExtension) override;
	VertexBufferDX12*	CreateVertexBuffer(Verts const& verts, std::string const& name = "New Vertex Buffer");
	VertexBufferDX12*	CreateVertexBuffer(VertTBNs const& verts, std::string const& name = "New Vertex Buffer");
	IndexBufferDX12*	CreateIndexBuffer(std::vector<unsigned int> indexes, std::string const& name = "New Index Buffer");
	ShaderDX12*			CreateOrGetShaderDX12FromFile(char const* shaderName, VertexType vertexType = VertexType::VERTEX_PCU);
	ConstantBufferDX12* CreateConstantBuffer(const unsigned int size);

	PipelineStateObjectDX12* CreateOrGetPSO(ShaderDX12* shader, RootSignatureDX12* rootSignature, BlendMode blendMode = BlendMode::OPAQUE, DepthMode depthMode = DepthMode::READ_WRITE_LESS_EQUAL,
		RasterizerMode rasterizerMode = RasterizerMode::SOLID_CULL_BACK);


	virtual void			SetModelConstants(Mat44 const& modelToWorldTransform = Mat44(), Rgba8 const& modelColor = Rgba8::WHITE) override;
	virtual void			SetLightConstants(Vec3 const& sunDirection, float sunIntensity, float ambientIntensity, Rgba8 const& sunColor = Rgba8::WHITE) override;
	virtual void			SetLightConstants(LightConstants const& lightConstants) override;
	virtual void			SetColorAdjustmentConstants(ColorAdjustmentConstants const& colorAdjustmentConstants) override;
	virtual void			SetPerFrameConstants(PerFrameConstants const& perFrameConstants) override;

	CommandQueue*			GetCommandQueue(D3D12_COMMAND_LIST_TYPE type) const;
	ID3D12Device2*			GetDevice() const;
	unsigned int			GetCurrentBackBufferIndex() const;
	ID3D12Resource*			GetCurrentBackBuffer() const;
	D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRenderTargetView() const;
	D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentDepthStencilView() const;
	DescriptorAllocator*		GetDescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE type) const;


	
	void				BindConstantBuffer(ID3D12GraphicsCommandList* commandList, int rootParameterIndex, ConstantBufferDX12* cbo);

	/*
	void				CopyCPUToGPU(const void* data, unsigned int size, VertexBufferDX12* vbo);
	void				CopyCPUToGPU(const void* data, unsigned int size, ConstantBufferDX12* cbo);
	void					UpdateBufferResource(ID3D12GraphicsCommandList2* commandList, ID3D12Resource** pDestinationResource, ID3D12Resource** pIntermediateResource, 
								size_t numElements, size_t elementSize, const void* bufferData, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);
	*/
private:
	ID3D12Device2*			CreateDevice(IDXGIAdapter4* adapter);
	IDXGIAdapter4*			GetAdapter(bool useWarp);
	bool					CheckTearingSupport();
	IDXGISwapChain4*		CreateSwapChain(HWND hWnd, ID3D12CommandQueue* commandQueue, uint32_t width, uint32_t height, uint32_t bufferCount);
	ID3D12DescriptorHeap*	CreateDescriptorHeap(ID3D12Device2* device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors);
	ID3D12CommandAllocator* CreateCommandAllocator(ID3D12Device2* device, D3D12_COMMAND_LIST_TYPE type);
	ID3D12GraphicsCommandList* CreateCommandList(ID3D12Device2* device, ID3D12CommandAllocator* commandAllocator, D3D12_COMMAND_LIST_TYPE type);
	ID3D12Fence*			CreateFence(ID3D12Device2* device);
	HANDLE					CreateEventHandle();
	ShaderDX12*				CreateShaderDX12(char const* shaderName, char const* shaderSource, VertexType vertexType = VertexType::VERTEX_PCU);
	ShaderDX12*				CreateShaderDX12(char const* shaderName, VertexType vertexType = VertexType::VERTEX_PCU);
	RootSignatureDX12*		CreateDefaultRootSignature(); //#TODO DX12 figure out needed inputs for root signature
	ID3D12DescriptorHeap*	CreateDescriptorHeapForDepthStencilView();
	PipelineStateObjectDX12* CreatePSO(ShaderDX12* shader, RootSignatureDX12* rootSignature, 
		BlendMode blendMode = BlendMode::OPAQUE, DepthMode depthMode = DepthMode::READ_WRITE_LESS_EQUAL, RasterizerMode rasterizerMode = RasterizerMode::SOLID_CULL_BACK);
	void					ResizeDepthBuffer(int width, int height);


	void					EnableDebugLayer();
	void					UpdateRenderTargetViews(ID3D12Device2* device, IDXGISwapChain4* swapChain, ID3D12DescriptorHeap* descriptorHeap);
	void					TransitionResource(CommandList* commandList, ID3D12Resource* resource, D3D12_RESOURCE_STATES beforeState, D3D12_RESOURCE_STATES afterState) const;
	void					ClearRTV(D3D12_CPU_DESCRIPTOR_HANDLE rtv, float* clearColor) const;
	void					ClearDepthStencilView(D3D12_CPU_DESCRIPTOR_HANDLE dsv, float depth = 1.0f) const;
	std::string				GetPSOName(ShaderDX12* shader, RootSignatureDX12* rootSignature, BlendMode blendMode, DepthMode depthMode, RasterizerMode rasterizerMode) const;



public:


private:
	bool m_vSync = true;
	bool m_tearingSupported = false;
	bool m_useWarp = false;

	ShaderDX12*					m_defaultShader = nullptr;
	std::vector<ShaderDX12*>	m_loadedShaders;
	RootSignatureDX12*			m_defaultRootSignature = nullptr;
	std::vector<PipelineStateObjectDX12*> m_loadedPipelineStateObjects;


	CommandQueue* m_directCommandQueue = nullptr;
	CommandQueue* m_computeCommandQueue = nullptr;
	CommandQueue* m_copyCommandQueue = nullptr;

	CommandList* m_directCommandList = nullptr;
	CommandList* m_copyCommandList = nullptr;
	CommandList* m_computeCommandList = nullptr;

	ID3D12Device2*			m_device = nullptr;
	IDXGISwapChain4*		m_swapChain = nullptr;
	ID3D12Resource*			m_backBuffers[NUM_BUFFER_FRAMES] = {};
	ID3D12DescriptorHeap*	m_rtvDescriptorHeap = nullptr;
	ID3D12Resource*			m_depthBuffer = nullptr;
	ID3D12DescriptorHeap*	m_dSVHeap = nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE m_rtvDescriptorHeapStartHandle = {};
	unsigned int			m_rtvDescriptorSize = 0;
	unsigned int			m_currentBackBufferIndex = 0;
	
	uint64_t				m_frameFenceValues[NUM_BUFFER_FRAMES] = {};

	DescriptorAllocator*	m_descriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

	/*
	ConstantBufferDX12*		m_cameraCBO = nullptr;
	ConstantBufferDX12*		m_modelCBO = nullptr;
	ConstantBufferDX12*		m_lightCBO = nullptr;
	ConstantBufferDX12*		m_perFrameCBO = nullptr;
	*/
};

