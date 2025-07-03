#pragma once
#include "Engine/Renderer/ThreadSafeQueue.hpp"
#include <d3d12.h>
#include <cstdint> 
#include <queue>
#include <cstdint> 
#include <memory>
#include <atomic>  
#include <thread>
#include <mutex>
#include <condition_variable>


class CommandList;
class RendererDX12;

constexpr unsigned int MAX_NUM_COMMAND_LISTS = 5;
class CommandQueue
{
public:
	CommandQueue(RendererDX12 const* renderer, D3D12_COMMAND_LIST_TYPE type);
	virtual ~CommandQueue();

	// Get an available command list from the command queue.
	CommandList* GetCommandList();

	// Execute a command list.
	// Returns the fence value to wait for for this command list.
	uint64_t ExecuteCommandList(CommandList* commandList);
	uint64_t ExecuteCommandLists(std::vector<CommandList*> const& commandLists);

	uint64_t Signal();
	bool IsFenceComplete(uint64_t fenceValue);
	void WaitForFenceValue(uint64_t fenceValue);
	void Flush();

	void Wait(CommandQueue const& other);

	ID3D12CommandQueue* GetD3D12CommandQueue() const;

protected:


private:
	void ProcessInFlightCommandLists();
	void InitializeCommandLists(std::string const& name);



private:
	// Keep track of command allocators that are "in-flight"
	struct CommandAllocatorEntry
	{
		uint64_t fenceValue;
		ID3D12CommandAllocator* commandAllocator;
	};

	using CommandAllocatorQueue = std::queue<CommandAllocatorEntry>;

	D3D12_COMMAND_LIST_TYPE                     m_commandListType;
	RendererDX12 const*							m_renderer = nullptr;
	ID3D12CommandQueue*							m_d3d12CommandQueue = nullptr;
	ID3D12Fence*								m_d3d12Fence = nullptr;
	std::atomic_uint64_t                        m_fenceValue;


	using CommandListEntry = std::tuple<uint64_t, CommandList* >;
	ThreadSafeQueue<CommandListEntry>               m_InFlightCommandLists;
	ThreadSafeQueue<CommandList*>					m_AvailableCommandLists;
	


	std::thread m_ProcessInFlightCommandListsThread;
	std::atomic_bool m_bProcessInFlightCommandLists;
	std::mutex m_ProcessInFlightCommandListsThreadMutex;
	std::condition_variable m_ProcessInFlightCommandListsThreadCV;
};

