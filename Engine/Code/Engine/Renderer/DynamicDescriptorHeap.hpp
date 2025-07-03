#pragma once
#include <cstdint>
#include <memory>
#include <queue>
#include <d3d12.h>
#include <functional>

class CommandList;
class RootSignatureDX12;
class RendererDX12;

/**
 * A structure that represents a descriptor table entry in the root signature.
 */
struct DescriptorTableCache
{
    DescriptorTableCache()
        : NumDescriptors(0)
        , BaseDescriptor(nullptr)
    {}

    // Reset the table cache.
    void Reset()
    {
        NumDescriptors = 0;
        BaseDescriptor = nullptr;
    }

    // The number of descriptors in this descriptor table.
    uint32_t NumDescriptors;
    // The pointer to the descriptor in the descriptor handle cache.
    D3D12_CPU_DESCRIPTOR_HANDLE* BaseDescriptor;
};

class DynamicDescriptorHeap
{
public:
	DynamicDescriptorHeap(RendererDX12 const* renderer, D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint32_t numDescriptorsPerHeap = 1024);
	virtual ~DynamicDescriptorHeap();

    #pragma region Explanation...
    /**
 * Stages a contiguous range of CPU visible descriptors.
 * Descriptors are not copied to the GPU visible descriptor heap until
 * the CommitStagedDescriptors function is called.
 */
    #pragma endregion
    void StageDescriptors(uint32_t rootParameterIndex, uint32_t offset, uint32_t numDescriptors, const D3D12_CPU_DESCRIPTOR_HANDLE srcDescriptors);

    #pragma region Explanation...
    /**
 * Copy all of the staged descriptors to the GPU visible descriptor heap and
 * bind the descriptor heap and the descriptor tables to the command list.
 * The passed-in function object is used to set the GPU visible descriptors
 * on the command list. Two possible functions are:
 *   * Before a draw    : ID3D12GraphicsCommandList::SetGraphicsRootDescriptorTable
 *   * Before a dispatch: ID3D12GraphicsCommandList::SetComputeRootDescriptorTable
 *
 * Since the DynamicDescriptorHeap can't know which function will be used, it must
 * be passed as an argument to the function.
 */
 #pragma endregion
    void CommitStagedDescriptors(CommandList& commandList, std::function<void(ID3D12GraphicsCommandList*, UINT, D3D12_GPU_DESCRIPTOR_HANDLE)> setFunc);
    void CommitStagedDescriptorsForDraw(CommandList& commandList);
    void CommitStagedDescriptorsForDispatch(CommandList& commandList);

    #pragma region Explanation...
    /**
 * Copies a single CPU visible descriptor to a GPU visible descriptor heap.
 * This is useful for the
 *   * ID3D12GraphicsCommandList::ClearUnorderedAccessViewFloat
 *   * ID3D12GraphicsCommandList::ClearUnorderedAccessViewUint
 * methods which require both a CPU and GPU visible descriptors for a UAV
 * resource.
 *
 * @param commandList The command list is required in case the GPU visible
 * descriptor heap needs to be updated on the command list.
 * @param cpuDescriptor The CPU descriptor to copy into a GPU visible
 * descriptor heap.
 *
 * @return The GPU visible descriptor.
 */
 #pragma endregion
    D3D12_GPU_DESCRIPTOR_HANDLE CopyDescriptor(CommandList& comandList, D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptor);
    #pragma region Explanation...
    /**
 * Parse the root signature to determine which root parameters contain
 * descriptor tables and determine the number of descriptors needed for
 * each table.
 */
    #pragma endregion
    void ParseRootSignature(RootSignatureDX12 const& rootSignature);

    #pragma region Explanation...
    /**
 * Reset used descriptors. This should only be done if any descriptors
 * that are being referenced by a command list has finished executing on the
 * command queue.
 */
    #pragma endregion
    void Reset();

private:
	// Request a descriptor heap if one is available.
	ID3D12DescriptorHeap* RequestDescriptorHeap();
	// Create a new descriptor heap of no descriptor heap is available.
	ID3D12DescriptorHeap* CreateDescriptorHeap();

	// Compute the number of stale descriptors that need to be copied to GPU visible descriptor heap.
    uint32_t ComputeStaleDescriptorCount() const;


private:
    RendererDX12 const* m_renderer = nullptr;

    #pragma region Explanation...
    /**
 * The maximum number of descriptor tables per root signature.
 * A 32-bit mask is used to keep track of the root parameter indices that
 * are descriptor tables.
 */
    #pragma endregion
    static const uint32_t MaxDescriptorTables = 32;
    #pragma region Explanation...
	// Describes the type of descriptors that can be staged using this 
// dynamic descriptor heap.
// Valid values are:
//   * D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
//   * D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER
// This parameter also determines the type of GPU visible descriptor heap to 
// create.
    #pragma endregion
	D3D12_DESCRIPTOR_HEAP_TYPE m_DescriptorHeapType;

	// The number of descriptors to allocate in new GPU visible descriptor heaps.
	uint32_t m_NumDescriptorsPerHeap;

	// The increment size of a descriptor.
	uint32_t m_DescriptorHandleIncrementSize;

	// The descriptor handle cache.
	std::unique_ptr<D3D12_CPU_DESCRIPTOR_HANDLE[]> m_DescriptorHandleCache;

	// Descriptor handle cache per descriptor table.
	DescriptorTableCache m_DescriptorTableCache[MaxDescriptorTables];

	// Each bit in the bit mask represents the index in the root signature that contains a descriptor table.
	uint32_t m_DescriptorTableBitMask;
	// Each bit set in the bit mask represents a descriptor table in the root signature that has changed since the last time the descriptors were copied.
	uint32_t m_StaleDescriptorTableBitMask;

	using DescriptorHeapPool = std::queue< ID3D12DescriptorHeap* >;

	DescriptorHeapPool m_DescriptorHeapPool;
	DescriptorHeapPool m_AvailableDescriptorHeaps;

	ID3D12DescriptorHeap* m_CurrentDescriptorHeap = nullptr;
    D3D12_GPU_DESCRIPTOR_HANDLE  m_CurrentGPUDescriptorHandle;
    D3D12_CPU_DESCRIPTOR_HANDLE  m_CurrentCPUDescriptorHandle;

	uint32_t m_NumFreeHandles;

};


