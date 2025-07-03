#include "Engine/Renderer/CommandList.hpp"
#include "Engine/Renderer/DynamicDescriptorHeap.hpp"
#include "Engine/Renderer/ResourceStateTracker.hpp"
#include "Engine/Renderer/UploadBufferDX12.hpp"
#include "Engine/Renderer/RendererDX12.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Renderer/ResourceDX12.hpp"
#include "Engine/Renderer/VertexBufferDX12.hpp"
#include "Engine/Renderer/IndexBufferDX12.hpp"
#include "Engine/Renderer/RootSignatureDX12.hpp"
#include "Engine/Renderer/PipelineStateObjectDX12.hpp"
#include "Engine/Renderer/ConstantBufferDX12.hpp"
#include <cassert>

CommandList::CommandList(RendererDX12 const* renderer, D3D12_COMMAND_LIST_TYPE type, std::string const& name)
	:m_d3d12CommandListType(type)
	,m_renderer(renderer)
{
	ID3D12Device* device = m_renderer->GetDevice();

	HRESULT hr = device->CreateCommandAllocator(m_d3d12CommandListType, IID_PPV_ARGS(&m_d3d12CommandAllocator));
	GUARANTEE_OR_DIE(SUCCEEDED(hr), "could not create command allocator");
	SetCommandAllocatorName(m_d3d12CommandAllocator, Stringf("%s - Command Allocator", name.c_str()));

	hr = device->CreateCommandList(0, m_d3d12CommandListType, m_d3d12CommandAllocator, nullptr, IID_PPV_ARGS(&m_d3d12CommandList));
	GUARANTEE_OR_DIE(SUCCEEDED(hr), "could not create command list");
	SetCommandListName(m_d3d12CommandList, name);

	m_UploadBuffer = std::make_unique<UploadBufferDX12>(m_renderer);

	m_ResourceStateTracker = std::make_unique<ResourceStateTracker>();

	for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		m_DynamicDescriptorHeap[i] = std::make_unique<DynamicDescriptorHeap>(m_renderer, static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(i));
		m_DescriptorHeaps[i] = nullptr;
	}
}

CommandList::~CommandList()
{
	DX_SAFE_RELEASE(m_d3d12CommandAllocator);
	DX_SAFE_RELEASE(m_d3d12CommandList);
	DX_SAFE_RELEASE(m_RootSignature);

	for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		DX_SAFE_RELEASE(m_DescriptorHeaps[i]);
	}

	for (int i = 0; i < (int)m_TrackedObjects.size(); ++i)
	{
		DX_SAFE_RELEASE(m_TrackedObjects[i]);
	}

	m_ResourceStateTracker->Reset();
	m_UploadBuffer->Reset();
	for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		m_DynamicDescriptorHeap[i].reset();
	}
}

void CommandList::TransitionBarrier(const ResourceDX12& resource, D3D12_RESOURCE_STATES stateAfter, unsigned int subResource, bool flushBarriers)
{
	ID3D12Resource* d3d12Resource = resource.GetD3D12Resource();
	if (d3d12Resource)
	{
		//D3D12_RESOURCE_STATES currentState = m_ResourceStateTracker->GetResourceState(d3d12Resource, subResource);
		//auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(d3d12Resource.Get(), D3D12_RESOURCE_STATE_COMMON, stateAfter, subResource);
		// The "before" state is not important. It will be resolved by the resource state tracker.

		//if (currentState != stateAfter)
		{
			D3D12_RESOURCE_BARRIER barrier = {};
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barrier.Transition.pResource = d3d12Resource;
			barrier.Transition.Subresource = subResource;
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
			barrier.Transition.StateAfter = stateAfter;
			m_ResourceStateTracker->ResourceBarrier(barrier);
		}
	}

	if (flushBarriers)
	{
		FlushResourceBarriers();
	}
}

void CommandList::CopyResource(ResourceDX12& dstRes, const ResourceDX12& srcRes)
{
	TransitionBarrier(dstRes, D3D12_RESOURCE_STATE_COPY_DEST);
	TransitionBarrier(srcRes, D3D12_RESOURCE_STATE_COPY_SOURCE);

	FlushResourceBarriers();

	m_d3d12CommandList->CopyResource(dstRes.GetD3D12Resource(), srcRes.GetD3D12Resource());

	TrackResource(dstRes);
	TrackResource(srcRes);
}

void CommandList::CopyBuffer(BufferDX12& buffer, size_t numElements, size_t elementSize, const void* bufferData, D3D12_RESOURCE_FLAGS flags)
{
	ID3D12Device* device = m_renderer->GetDevice();
	size_t bufferSize = numElements * elementSize;
	ID3D12Resource* d3d12Resource = nullptr;

	if (bufferSize != 0) // if buffer size == 0 a "null resource" will be created
	{

		D3D12_HEAP_PROPERTIES heapProps = {};
		heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
		heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heapProps.CreationNodeMask = 1;
		heapProps.VisibleNodeMask = 1;

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

		HRESULT hr =  device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&d3d12Resource));
		GUARANTEE_OR_DIE(SUCCEEDED(hr), "could not create committed resource in CommandList::CopyBuffer()");

		ResourceStateTracker::AddGlobalResourceState(d3d12Resource, D3D12_RESOURCE_STATE_COMMON);

		if (bufferData != nullptr)
		{
			ID3D12Resource* uploadResource = nullptr;

			D3D12_HEAP_PROPERTIES heapPropsUploadResource = {};
			heapPropsUploadResource.Type = D3D12_HEAP_TYPE_UPLOAD;
			heapPropsUploadResource.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			heapPropsUploadResource.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
			heapPropsUploadResource.CreationNodeMask = 1;
			heapPropsUploadResource.VisibleNodeMask = 1;

			D3D12_RESOURCE_DESC resourceDescUploadResource = {};
			resourceDescUploadResource.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			resourceDescUploadResource.Alignment = 0;
			resourceDescUploadResource.Width = bufferSize;
			resourceDescUploadResource.Height = 1;
			resourceDescUploadResource.DepthOrArraySize = 1;
			resourceDescUploadResource.MipLevels = 1;
			resourceDescUploadResource.Format = DXGI_FORMAT_UNKNOWN;
			resourceDescUploadResource.SampleDesc.Count = 1;
			resourceDescUploadResource.SampleDesc.Quality = 0;
			resourceDescUploadResource.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			resourceDescUploadResource.Flags = D3D12_RESOURCE_FLAG_NONE;

			hr = device->CreateCommittedResource(&heapPropsUploadResource, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uploadResource));
			GUARANTEE_OR_DIE(SUCCEEDED(hr), "could not create committed resource for upload resouce in CommandList::CopyBuffer()");

			D3D12_SUBRESOURCE_DATA subresourceData = {};
			subresourceData.pData = bufferData;
			subresourceData.RowPitch = bufferSize;
			subresourceData.SlicePitch = subresourceData.RowPitch;

			m_ResourceStateTracker->TransitionResource(d3d12Resource, D3D12_RESOURCE_STATE_COPY_DEST);
			FlushResourceBarriers();

			UpdateSubresources(m_d3d12CommandList, d3d12Resource, uploadResource, 0, 0, 1, &subresourceData);

			SetResourceName(uploadResource, "Upload_Resource");
			TrackObject(uploadResource);
			TrackIntermediateObject(uploadResource);
		}

		TrackObject(d3d12Resource);
	}

	buffer.SetD3D12Resource(d3d12Resource);
	buffer.CreateViews(numElements, elementSize);
}

void CommandList::CopyVertexBuffer(VertexBufferDX12& vertexBuffer, size_t numVertices, size_t vertexStride, const void* vertexBufferData)
{
	CopyBuffer(vertexBuffer, numVertices, vertexStride, vertexBufferData);
}

void CommandList::CopyIndexBuffer(IndexBufferDX12& indexBuffer, size_t numIndexes, DXGI_FORMAT indexFormat, const void* indexBufferData)
{
	size_t indexSizeInBytes = indexFormat == DXGI_FORMAT_R16_UINT ? 2 : 4;
	CopyBuffer(indexBuffer, numIndexes, indexSizeInBytes, indexBufferData);
}

void CommandList::FlushResourceBarriers()
{
	m_ResourceStateTracker->FlushResourceBarriers(*this);
}

void CommandList::SetGraphicsDynamicConstantBuffer(uint32_t rootParameterIndex, size_t sizeInBytes, const void* bufferData)
{
	// Constant buffers must be 256-byte aligned.
	Allocation heapAllococation = m_UploadBuffer->Allocate(sizeInBytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	memcpy(heapAllococation.CPU, bufferData, sizeInBytes);

	m_d3d12CommandList->SetGraphicsRootConstantBufferView(rootParameterIndex, heapAllococation.GPU);
}

void CommandList::SetGraphics32BitConstants(uint32_t rootParameterIndex, uint32_t numConstants, const void* constants)
{
	m_d3d12CommandList->SetGraphicsRoot32BitConstants(rootParameterIndex, numConstants, constants, 0);
}
void CommandList::SetGraphicsRootConstantBufferView(uint32_t rootParameterIndex, ConstantBufferDX12 const* constantBuffer)
{
	ID3D12Resource* resource = constantBuffer->GetD3D12Resource();
	if (!resource)
	{
		ERROR_AND_DIE("Trying to set Constant buffer with null resource CommandList::SetGraphicsRootConstantBufferView()");
	}

	m_d3d12CommandList->SetGraphicsRootConstantBufferView(rootParameterIndex, resource->GetGPUVirtualAddress());
}

void CommandList::SetCompute32BitConstants(uint32_t rootParameterIndex, uint32_t numConstants, const void* constants)
{
	m_d3d12CommandList->SetComputeRoot32BitConstants(rootParameterIndex, numConstants, constants, 0);
}

void CommandList::SetShaderResourceView(uint32_t rootParameterIndex, uint32_t descriptorOffset, const ResourceDX12& resource, D3D12_RESOURCE_STATES stateAfter,
	unsigned int firstSubresource, unsigned int numSubresources, const D3D12_SHADER_RESOURCE_VIEW_DESC* srv)
{
	if (numSubresources < D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
	{
		for (uint32_t i = 0; i < numSubresources; ++i)
		{
			TransitionBarrier(resource, stateAfter, firstSubresource + i);
		}
	}
	else
	{
		TransitionBarrier(resource, stateAfter);
	}

	m_DynamicDescriptorHeap[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->StageDescriptors(rootParameterIndex, descriptorOffset, 1, resource.GetShaderResourceView(srv));

	TrackResource(resource);
}

void CommandList::SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY primitiveTopology)
{
	m_d3d12CommandList->IASetPrimitiveTopology(primitiveTopology);
}

void CommandList::SetVertexBuffer(uint32_t slot, VertexBufferDX12 const& vertexBuffer)
{
	TransitionBarrier(vertexBuffer, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView = vertexBuffer.GetVertexBufferView();
	m_d3d12CommandList->IASetVertexBuffers(slot, 1, &vertexBufferView);
	TrackResource(vertexBuffer);
}

void CommandList::SetDynamicVertexBuffer(uint32_t slot, size_t numVertices, size_t vertexSize, const void* vertexBufferData)
{
	size_t bufferSize = numVertices * vertexSize;

	Allocation heapAllocation = m_UploadBuffer->Allocate(bufferSize, vertexSize);
	memcpy(heapAllocation.CPU, vertexBufferData, bufferSize);

	D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};
	vertexBufferView.BufferLocation = heapAllocation.GPU;
	vertexBufferView.SizeInBytes = static_cast<unsigned int>(bufferSize);
	vertexBufferView.StrideInBytes = static_cast<unsigned int>(vertexSize);

	m_d3d12CommandList->IASetVertexBuffers(slot, 1, &vertexBufferView);
}

void CommandList::SetIndexBuffer(IndexBufferDX12 const& indexBuffer)
{
	TransitionBarrier( indexBuffer, D3D12_RESOURCE_STATE_INDEX_BUFFER);
	D3D12_INDEX_BUFFER_VIEW indexBufferView = indexBuffer.GetIndexBufferView();
	m_d3d12CommandList->IASetIndexBuffer(&indexBufferView);
	TrackResource(indexBuffer);
}

void CommandList::SetDynamicIndexBuffer(size_t numIndicies, DXGI_FORMAT indexFormat, const void* indexBufferData)
{
	size_t indexSizeInBytes = indexFormat == DXGI_FORMAT_R16_UINT ? 2 : 4;
	size_t bufferSize = numIndicies * indexSizeInBytes;

	Allocation heapAllocation = m_UploadBuffer->Allocate(bufferSize, indexSizeInBytes);
	memcpy(heapAllocation.CPU, indexBufferData, bufferSize);

	D3D12_INDEX_BUFFER_VIEW indexBufferView = {};
	indexBufferView.BufferLocation = heapAllocation.GPU;
	indexBufferView.SizeInBytes = static_cast<unsigned int>(bufferSize);
	indexBufferView.Format = indexFormat;

	m_d3d12CommandList->IASetIndexBuffer(&indexBufferView);
}

void CommandList::SetViewport(const D3D12_VIEWPORT& viewport)
{
	SetViewports({ viewport });
}

void CommandList::SetViewports(const std::vector<D3D12_VIEWPORT>& viewports)
{
	assert(viewports.size() < D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE);
	m_d3d12CommandList->RSSetViewports(static_cast<unsigned int>(viewports.size()), viewports.data());
}

void CommandList::SetScissorRect(const D3D12_RECT& scissorRect)
{
	SetScissorRects({ scissorRect });
}

void CommandList::SetScissorRects(const std::vector<D3D12_RECT>& scissorRects)
{
	assert(scissorRects.size() < D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE);
	m_d3d12CommandList->RSSetScissorRects(static_cast<unsigned int>(scissorRects.size()), scissorRects.data());
}

void CommandList::SetPipelineState(ID3D12PipelineState* pipelineState)
{
	m_d3d12CommandList->SetPipelineState(pipelineState);
	TrackObject(pipelineState);
}

void CommandList::SetGraphicsRootSignature(const RootSignatureDX12& rootSignature)
{
	ID3D12RootSignature* d3d12RootSignature = rootSignature.GetRootSignature();
	if (m_RootSignature != d3d12RootSignature)
	{
		m_RootSignature = d3d12RootSignature;

		for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
		{
			m_DynamicDescriptorHeap[i]->ParseRootSignature(rootSignature);
		}

		m_d3d12CommandList->SetGraphicsRootSignature(m_RootSignature);

		TrackObject(m_RootSignature);
	}
}

void CommandList::SetComputeRootSignature(const RootSignatureDX12& rootSignature)
{
	ID3D12RootSignature* d3d12RootSignature = rootSignature.GetRootSignature();
	if (m_RootSignature != d3d12RootSignature)
	{
		m_RootSignature = d3d12RootSignature;

		for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
		{
			m_DynamicDescriptorHeap[i]->ParseRootSignature(rootSignature);
		}

		m_d3d12CommandList->SetComputeRootSignature(m_RootSignature);

		TrackObject(m_RootSignature);
	}
}

void CommandList::Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t startVertex, uint32_t startInstance)
{
	FlushResourceBarriers();

	for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		m_DynamicDescriptorHeap[i]->CommitStagedDescriptorsForDraw(*this);
	}

	m_d3d12CommandList->DrawInstanced(vertexCount, instanceCount, startVertex, startInstance);
}

void CommandList::DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t startIndex, int32_t baseVertex, uint32_t startInstance)
{
	FlushResourceBarriers();
	for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		m_DynamicDescriptorHeap[i]->CommitStagedDescriptorsForDraw(*this);
	}

	m_d3d12CommandList->DrawIndexedInstanced(indexCount, instanceCount, startIndex, baseVertex, startInstance);
}

void CommandList::Dispatch(uint32_t numGroupsX, uint32_t numGroupsY, uint32_t numGroupsZ)
{
	FlushResourceBarriers();

	for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		m_DynamicDescriptorHeap[i]->CommitStagedDescriptorsForDispatch(*this);
	}

	m_d3d12CommandList->Dispatch(numGroupsX, numGroupsY, numGroupsZ);
}

bool CommandList::Close(CommandList& pendingCommandList)
{
	// Flush any remaining barriers.
	FlushResourceBarriers();

	m_d3d12CommandList->Close();

	// Flush pending resource barriers.
	uint32_t numPendingBarriers = m_ResourceStateTracker->FlushPendingResourceBarriers(pendingCommandList);
	// Commit the final resource state to the global state.
	m_ResourceStateTracker->CommitFinalResourceStates();

	return numPendingBarriers > 0;
}

void CommandList::Close()
{
	FlushResourceBarriers();
	m_d3d12CommandList->Close();
}

void CommandList::Reset()
{
	HRESULT hr = m_d3d12CommandAllocator->Reset();

	GUARANTEE_OR_DIE(SUCCEEDED(hr), "Could Not Reset command allocator");

	hr = m_d3d12CommandList->Reset(m_d3d12CommandAllocator, nullptr);
	GUARANTEE_OR_DIE(SUCCEEDED(hr), "could not reset command list");

	m_ResourceStateTracker->Reset();
	m_UploadBuffer->Reset();

	ReleaseTrackedObjects();

	for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		m_DynamicDescriptorHeap[i]->Reset();
		m_DescriptorHeaps[i] = nullptr;
	}

	m_RootSignature = nullptr;
	//m_ComputeCommandList = nullptr;
}

void CommandList::ReleaseTrackedObjects()
{
	m_TrackedObjects.clear();

	for (int i = 0; i < (int)m_intermediateObjects.size(); ++i)
	{
		if (m_intermediateObjects[i])
		{
			DX_SAFE_RELEASE(m_intermediateObjects[i]);
		}
	}
}

void CommandList::SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, ID3D12DescriptorHeap* heap)
{
	if (m_DescriptorHeaps[heapType] != heap)
	{
		m_DescriptorHeaps[heapType] = heap;
		BindDescriptorHeaps();
	}
}

void CommandList::TrackObject(ID3D12Object* object)
{
	m_TrackedObjects.push_back(object);
}

void CommandList::TrackResource(const ResourceDX12& res)
{
	TrackObject(res.GetD3D12Resource());
}

void CommandList::TrackIntermediateResource(ResourceDX12 const& res)
{
	TrackIntermediateObject(res.GetD3D12Resource());
}

void CommandList::TrackIntermediateObject(ID3D12Object* object)
{
	m_intermediateObjects.push_back(object);
}

void CommandList::BindDescriptorHeaps()
{
	unsigned int numDescriptorHeaps = 0;
	ID3D12DescriptorHeap* descriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES] = {};

	for (uint32_t i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		ID3D12DescriptorHeap* descriptorHeap = m_DescriptorHeaps[i];
		if (descriptorHeap)
		{
			descriptorHeaps[numDescriptorHeaps++] = descriptorHeap;
		}
	}

	m_d3d12CommandList->SetDescriptorHeaps(numDescriptorHeaps, descriptorHeaps);
}
