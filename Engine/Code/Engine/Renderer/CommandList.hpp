#pragma once
#include <d3d12.h>

#include <map> // for std::map
#include <memory> // for std::unique_ptr
#include <mutex> // for std::mutex
#include <vector> // for std::vector
#include <cassert>

class ResourceStateTracker;
class UploadBufferDX12;
class DynamicDescriptorHeap;
class RendererDX12;
class ResourceDX12;
class BufferDX12;
class ConstantBufferDX12;
class VertexBufferDX12;
class IndexBufferDX12;
class RootSignatureDX12;

class CommandList
{
public:
	CommandList(RendererDX12 const* renderer, D3D12_COMMAND_LIST_TYPE type, std::string const& name = "Added Command List");
	virtual ~CommandList();

	D3D12_COMMAND_LIST_TYPE		GetCommandListType() const { return m_d3d12CommandListType;}
	ID3D12GraphicsCommandList2* GetGraphicsCommandList() const {return m_d3d12CommandList;}

	#pragma region Explanation...
	/**
	* Transition a resource to a particular state.
	*
	* @param resource The resource to transition.
	* @param stateAfter The state to transition the resource to. The before state is resolved by the resource state tracker.
	* @param subresource The subresource to transition. By default, this is D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES which indicates that all subresources are transitioned to the same state.
	* @param flushBarriers Force flush any barriers. Resource barriers need to be flushed before a command (draw, dispatch, or copy) that expects the resource to be in a particular state can run.
	*/
	#pragma endregion
	void		TransitionBarrier(ResourceDX12 const& resource, D3D12_RESOURCE_STATES stateAfter, unsigned int subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, bool flushBarriers = false);

	//Copy DX12 Objects
	//-------------------------------------------------------------------------------------------------------------------------------------------------
	void		CopyResource(ResourceDX12& dstRes, ResourceDX12 const& srcRes);
	void		CopyBuffer(BufferDX12& buffer, size_t numElements, size_t elementSize, const void* bufferData, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);
	void		CopyVertexBuffer(VertexBufferDX12& vertexBuffer, size_t numVertices, size_t vertexStride, const void* vertexBufferData);
	void		CopyIndexBuffer(IndexBufferDX12& indexBuffer, size_t numIndexes, DXGI_FORMAT indexFormat, const void* indexBufferData);


	void		FlushResourceBarriers();

	void		SetGraphicsDynamicConstantBuffer(uint32_t rootParameterIndex, size_t sizeInBytes, const void* bufferData);
	void		SetGraphics32BitConstants(uint32_t rootParameterIndex, uint32_t numConstants, const void* constants);
	void		SetGraphicsRootConstantBufferView(uint32_t rootParameterIndex, ConstantBufferDX12 const* constantBuffer);

	void		SetCompute32BitConstants(uint32_t rootParameterIndex, uint32_t numConstants, const void* constants); // don't have need for this quite yet, but follows similar logic as Graphics32BitConstants so added for future use
	void		SetVertexBuffer(uint32_t slot, VertexBufferDX12 const& vertexBuffer);
	void		SetDynamicVertexBuffer(uint32_t slot, size_t numVertices, size_t vertexSize, const void* vertexBufferData);
	void		SetIndexBuffer(IndexBufferDX12 const& indexBuffer);
	void		SetDynamicIndexBuffer(size_t numIndicies, DXGI_FORMAT indexFormat, const void* indexBufferData);
	void		SetViewport(const D3D12_VIEWPORT& viewport);
	void		SetViewports(const std::vector<D3D12_VIEWPORT>& viewports);
	void		SetScissorRect(const D3D12_RECT& scissorRect);
	void		SetScissorRects(const std::vector<D3D12_RECT>& scissorRects);
	void		SetPipelineState(ID3D12PipelineState* pipelineState);
	void		SetGraphicsRootSignature(const RootSignatureDX12& rootSignature);
	void		SetComputeRootSignature(const RootSignatureDX12& rootSignature);

	void		SetShaderResourceView(uint32_t rootParameterIndex,uint32_t descriptorOffset, ResourceDX12 const& resource,D3D12_RESOURCE_STATES stateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
					unsigned int firstSubresource = 0,unsigned int numSubresources = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,const D3D12_SHADER_RESOURCE_VIEW_DESC* srv = nullptr);

	void		SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY primitiveTopology);
	//void		SetRenderTarget(const RenderTarget& renderTarget);
	void		SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, ID3D12DescriptorHeap* heap);

	void		Draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t startVertex = 0, uint32_t startInstance = 0);
	void		DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t startIndex = 0, int32_t baseVertex = 0, uint32_t startInstance = 0);
	void		Dispatch(uint32_t numGroupsX, uint32_t numGroupsY = 1, uint32_t numGroupsZ = 1); //Compute Shader Dispatch

	bool		Close(CommandList& pendingCommandList);
	void		Close();
	void		Reset();
	void		ReleaseTrackedObjects();



	template<typename T>
	void CopyVertexBuffer(VertexBufferDX12& vertexBuffer, std::vector<T> const& vertexBufferData)
	{
		CopyVertexBuffer(vertexBuffer, vertexBufferData.size(), sizeof(T), vertexBufferData.data());
	}
	template<typename T>
	void CopyIndexBuffer(IndexBufferDX12& indexBuffer, std::vector<T> const& indexBufferData)
	{
		assert(sizeof(T) == 2 || sizeof(T) == 4);

		DXGI_FORMAT indexFormat = (sizeof(T) == 2) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
		CopyIndexBuffer(indexBuffer, indexBufferData.size(), indexFormat, indexBufferData.data());
	}
	template<typename T>
	void SetGraphicsDynamicConstantBuffer(uint32_t rootParameterIndex, const T& data)
	{
		SetGraphicsDynamicConstantBuffer(rootParameterIndex, sizeof(T), &data);
	}
	template<typename T>
	void SetGraphics32BitConstants(uint32_t rootParameterIndex, const T& constants)
	{
		static_assert(sizeof(T) % sizeof(uint32_t) == 0, "Size of type must be a multiple of 4 bytes");
		SetGraphics32BitConstants(rootParameterIndex, sizeof(T) / sizeof(uint32_t), &constants);
	}
	template<typename T>
	void SetCompute32BitConstants(uint32_t rootParameterIndex, const T& constants)
	{
		static_assert(sizeof(T) % sizeof(uint32_t) == 0, "Size of type must be a multiple of 4 bytes");
		SetCompute32BitConstants(rootParameterIndex, sizeof(T) / sizeof(uint32_t), &constants);
	}
	template<typename T>
	void SetDynamicVertexBuffer(uint32_t slot, const std::vector<T>& vertexBufferData)
	{
		SetDynamicVertexBuffer(slot, vertexBufferData.size(), sizeof(T), vertexBufferData.data());
	}
	template<typename T>
	void SetDynamicIndexBuffer(const std::vector<T>& indexBufferData)
	{
		static_assert(sizeof(T) == 2 || sizeof(T) == 4);

		DXGI_FORMAT indexFormat = (sizeof(T) == 2) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
		SetDynamicIndexBuffer(indexBufferData.size(), indexFormat, indexBufferData.data());
	}
private:
	void		TrackObject(ID3D12Object* object);
	void		TrackResource(ResourceDX12 const& res);
	void		TrackIntermediateResource(ResourceDX12 const& res);
	void		TrackIntermediateObject(ID3D12Object* object);

	void		BindDescriptorHeaps();

private:
	RendererDX12 const*					m_renderer = nullptr;
	D3D12_COMMAND_LIST_TYPE				m_d3d12CommandListType;
	ID3D12GraphicsCommandList2*			m_d3d12CommandList = nullptr;
	ID3D12CommandAllocator*				m_d3d12CommandAllocator = nullptr;
	using TrackedObjects = std::vector <ID3D12Object*> ;
	TrackedObjects					m_TrackedObjects;
	using IntermediateObjects = std::vector<ID3D12Object*>;
	IntermediateObjects				m_intermediateObjects;


	std::unique_ptr<ResourceStateTracker> m_ResourceStateTracker;
	std::unique_ptr<UploadBufferDX12>	m_UploadBuffer;
	std::unique_ptr<DynamicDescriptorHeap> m_DynamicDescriptorHeap[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
	ID3D12DescriptorHeap*				m_DescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
	ID3D12RootSignature*				m_RootSignature = nullptr;


};

