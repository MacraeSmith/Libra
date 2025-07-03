#include "Engine/Renderer/DynamicDescriptorHeap.hpp"
#include "Engine/Renderer/RootSignatureDX12.hpp"
#include "Engine/Renderer/CommandList.hpp"
#include "Engine/Renderer/RendererDX12.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include <cassert>


DynamicDescriptorHeap::DynamicDescriptorHeap(RendererDX12 const* renderer, D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint32_t numDescriptorsPerHeap)
	: m_renderer(renderer)
	, m_DescriptorHeapType(heapType)
	, m_NumDescriptorsPerHeap(numDescriptorsPerHeap)
	, m_DescriptorTableBitMask(0)
	, m_StaleDescriptorTableBitMask(0)
	, m_CurrentCPUDescriptorHandle{}
	, m_CurrentGPUDescriptorHandle{}
	, m_NumFreeHandles(0)
{
	ID3D12Device* device = m_renderer->GetDevice();
	m_DescriptorHandleIncrementSize = device->GetDescriptorHandleIncrementSize(heapType);

	// Allocate space for staging CPU visible descriptors.
	m_DescriptorHandleCache = std::make_unique<D3D12_CPU_DESCRIPTOR_HANDLE[]>(m_NumDescriptorsPerHeap);
}

DynamicDescriptorHeap::~DynamicDescriptorHeap()
{
/*
	// Release descriptor heaps in the pool
	while (!m_DescriptorHeapPool.empty())
	{
		DX_SAFE_RELEASE(m_DescriptorHeapPool.front());
		m_DescriptorHeapPool.pop();
	}

	// Release any leftover available heaps
	while (!m_AvailableDescriptorHeaps.empty())
	{
		DX_SAFE_RELEASE(m_AvailableDescriptorHeaps.front());
		m_AvailableDescriptorHeaps.pop();
	}

	// Current heap, if held separately
	DX_SAFE_RELEASE(m_CurrentDescriptorHeap);
*/
}

void DynamicDescriptorHeap::ParseRootSignature(RootSignatureDX12 const& rootSignature)
{
	// If the root signature changes, all descriptors must be (re)bound to the command list.
	m_StaleDescriptorTableBitMask = 0;

	const auto& rootSignatureDesc = rootSignature.GetRootSignatureDesc();

	// Get a bit mask that represents the root parameter indices that match the descriptor heap type for this dynamic descriptor heap.
	m_DescriptorTableBitMask = rootSignature.GetDescriptorTableBitMask(m_DescriptorHeapType);

	uint32_t descriptorTableBitMask = m_DescriptorTableBitMask;
	uint32_t currentOffset = 0;
	DWORD rootIndex;
	while (_BitScanForward(&rootIndex, descriptorTableBitMask) && rootIndex < rootSignatureDesc.NumParameters)
	{
		uint32_t numDescriptors = rootSignature.GetNumDescriptors(rootIndex);

		DescriptorTableCache& descriptorTableCache = m_DescriptorTableCache[rootIndex];
		descriptorTableCache.NumDescriptors = numDescriptors;
		descriptorTableCache.BaseDescriptor = m_DescriptorHandleCache.get() + currentOffset;

		currentOffset += numDescriptors;

		// Flip the descriptor table bit so it's not scanned again for the current index.
		descriptorTableBitMask ^= (1 << rootIndex);
	}

	// Make sure the maximum number of descriptors per descriptor heap has not been exceeded.
	assert(currentOffset <= m_NumDescriptorsPerHeap && "The root signature requires more than the maximum number of descriptors per descriptor heap. Consider increasing the maximum number of descriptors per descriptor heap.");
}

void DynamicDescriptorHeap::StageDescriptors(uint32_t rootParameterIndex, uint32_t offset, uint32_t numDescriptors, const D3D12_CPU_DESCRIPTOR_HANDLE srcDescriptor)
{
	// Cannot stage more than the maximum number of descriptors per heap.
	// Cannot stage more than MaxDescriptorTables root parameters.
	if (numDescriptors > m_NumDescriptorsPerHeap || rootParameterIndex >= MaxDescriptorTables)
	{
		throw std::bad_alloc();
	}

	DescriptorTableCache& descriptorTableCache = m_DescriptorTableCache[rootParameterIndex];

	// Check that the number of descriptors to copy does not exceed the number of descriptors expected in the descriptor table.
	if ((offset + numDescriptors) > descriptorTableCache.NumDescriptors)
	{
		throw std::length_error("Number of descriptors exceeds the number of descriptors in the descriptor table.");
	}

	D3D12_CPU_DESCRIPTOR_HANDLE* dstDescriptor = (descriptorTableCache.BaseDescriptor + offset);
	for (uint32_t i = 0; i < numDescriptors; ++i)
	{
		//dstDescriptor[i] = CD3DX12_CPU_DESCRIPTOR_HANDLE(srcDescriptor, i, m_DescriptorHandleIncrementSize);
		dstDescriptor[i].ptr = srcDescriptor.ptr + static_cast<SIZE_T>(i) * m_DescriptorHandleIncrementSize;
	}

	// Set the root parameter index bit to make sure the descriptor table at that index is bound to the command list.
	m_StaleDescriptorTableBitMask |= (1 << rootParameterIndex);
}

uint32_t DynamicDescriptorHeap::ComputeStaleDescriptorCount() const
{
	uint32_t numStaleDescriptors = 0;
	DWORD i;
	DWORD staleDescriptorsBitMask = m_StaleDescriptorTableBitMask;

	while (_BitScanForward(&i, staleDescriptorsBitMask))
	{
		numStaleDescriptors += m_DescriptorTableCache[i].NumDescriptors;
		staleDescriptorsBitMask ^= (1 << i);
	}

	return numStaleDescriptors;
}

ID3D12DescriptorHeap* DynamicDescriptorHeap::RequestDescriptorHeap()
{
	ID3D12DescriptorHeap* descriptorHeap = nullptr;
	if (!m_AvailableDescriptorHeaps.empty())
	{
		descriptorHeap = m_AvailableDescriptorHeaps.front();
		m_AvailableDescriptorHeaps.pop();
	}
	else
	{
		descriptorHeap = CreateDescriptorHeap();
		m_DescriptorHeapPool.push(descriptorHeap);
	}

	return descriptorHeap;
}

ID3D12DescriptorHeap* DynamicDescriptorHeap::CreateDescriptorHeap()
{
	ID3D12Device* device = m_renderer->GetDevice();

	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
	descriptorHeapDesc.Type = m_DescriptorHeapType;
	descriptorHeapDesc.NumDescriptors = m_NumDescriptorsPerHeap;
	descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	ID3D12DescriptorHeap* descriptorHeap;
	HRESULT hr = device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap));
	GUARANTEE_OR_DIE(SUCCEEDED(hr), "could not create descriptor heap");

	return descriptorHeap;
}

void DynamicDescriptorHeap::CommitStagedDescriptors(CommandList& commandList, std::function<void(ID3D12GraphicsCommandList*, UINT, D3D12_GPU_DESCRIPTOR_HANDLE)> setFunc)
{
	// Compute the number of descriptors that need to be copied 
	uint32_t numDescriptorsToCommit = ComputeStaleDescriptorCount();
	if (numDescriptorsToCommit > 0)
	{
		ID3D12Device* device = m_renderer->GetDevice();
		auto d3d12GraphicsCommandList = commandList.GetGraphicsCommandList();
		assert(d3d12GraphicsCommandList != nullptr);

		if (!m_CurrentDescriptorHeap || m_NumFreeHandles < numDescriptorsToCommit)
		{
			m_CurrentDescriptorHeap = RequestDescriptorHeap();
			m_CurrentCPUDescriptorHandle = m_CurrentDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
			m_CurrentGPUDescriptorHandle = m_CurrentDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
			m_NumFreeHandles = m_NumDescriptorsPerHeap;

			commandList.SetDescriptorHeap(m_DescriptorHeapType, m_CurrentDescriptorHeap);

			// When updating the descriptor heap on the command list, all descriptor
			// tables must be (re)recopied to the new descriptor heap (not just
			// the stale descriptor tables).
			m_StaleDescriptorTableBitMask = m_DescriptorTableBitMask;
		}

		DWORD rootIndex;
		// Scan from LSB to MSB for a bit set in staleDescriptorsBitMask
		while (_BitScanForward(&rootIndex, m_StaleDescriptorTableBitMask))
		{
			UINT numSrcDescriptors = m_DescriptorTableCache[rootIndex].NumDescriptors;
			D3D12_CPU_DESCRIPTOR_HANDLE* pSrcDescriptorHandles = m_DescriptorTableCache[rootIndex].BaseDescriptor;

			D3D12_CPU_DESCRIPTOR_HANDLE pDestDescriptorRangeStarts[] =
			{
				m_CurrentCPUDescriptorHandle
			};

			UINT pDestDescriptorRangeSizes[] =
			{
				numSrcDescriptors
			};

			// Copy the staged CPU visible descriptors to the GPU visible descriptor heap.
			device->CopyDescriptors(1, pDestDescriptorRangeStarts, pDestDescriptorRangeSizes,
				numSrcDescriptors, pSrcDescriptorHandles, nullptr, m_DescriptorHeapType);

			// Set the descriptors on the command list using the passed-in setter function.
			setFunc(d3d12GraphicsCommandList, rootIndex, m_CurrentGPUDescriptorHandle);

			// Offset current CPU and GPU descriptor handles.
			m_CurrentCPUDescriptorHandle.ptr += static_cast<SIZE_T>(numSrcDescriptors) * m_DescriptorHandleIncrementSize;
			m_CurrentGPUDescriptorHandle.ptr += static_cast<SIZE_T>(numSrcDescriptors) * m_DescriptorHandleIncrementSize;
			m_NumFreeHandles -= numSrcDescriptors;
			// Flip the stale bit so the descriptor table is not recopied again unless it is updated with a new descriptor.
			m_StaleDescriptorTableBitMask ^= (1 << rootIndex);
		}
	}
}

void DynamicDescriptorHeap::CommitStagedDescriptorsForDraw(CommandList& commandList)
{
	CommitStagedDescriptors(commandList, &ID3D12GraphicsCommandList::SetGraphicsRootDescriptorTable);
}

void DynamicDescriptorHeap::CommitStagedDescriptorsForDispatch(CommandList& commandList)
{
	CommitStagedDescriptors(commandList, &ID3D12GraphicsCommandList::SetComputeRootDescriptorTable);
}

D3D12_GPU_DESCRIPTOR_HANDLE DynamicDescriptorHeap::CopyDescriptor(CommandList& comandList, D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptor)
{
	if (!m_CurrentDescriptorHeap || m_NumFreeHandles < 1)
	{
		m_CurrentDescriptorHeap = RequestDescriptorHeap();
		m_CurrentCPUDescriptorHandle = m_CurrentDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		m_CurrentGPUDescriptorHandle = m_CurrentDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
		m_NumFreeHandles = m_NumDescriptorsPerHeap;

		comandList.SetDescriptorHeap(m_DescriptorHeapType, m_CurrentDescriptorHeap);

		// When updating the descriptor heap on the command list, all descriptor
		// tables must be (re)recopied to the new descriptor heap (not just
		// the stale descriptor tables).
		m_StaleDescriptorTableBitMask = m_DescriptorTableBitMask;
	}

	ID3D12Device* device = m_renderer->GetDevice();

	D3D12_GPU_DESCRIPTOR_HANDLE hGPU = m_CurrentGPUDescriptorHandle;
	device->CopyDescriptorsSimple(1, m_CurrentCPUDescriptorHandle, cpuDescriptor, m_DescriptorHeapType);

	m_CurrentCPUDescriptorHandle.ptr += static_cast<SIZE_T>(m_DescriptorHandleIncrementSize);
	m_CurrentGPUDescriptorHandle.ptr += static_cast<UINT64>(m_DescriptorHandleIncrementSize);
	m_NumFreeHandles -= 1;

	return hGPU;
}

void DynamicDescriptorHeap::Reset()
{
	m_AvailableDescriptorHeaps = m_DescriptorHeapPool;
	DX_SAFE_RELEASE(m_CurrentDescriptorHeap);
	m_CurrentCPUDescriptorHandle.ptr = 0;
	m_CurrentGPUDescriptorHandle.ptr = 0;
	m_NumFreeHandles = 0;
	m_DescriptorTableBitMask = 0;
	m_StaleDescriptorTableBitMask = 0;
}