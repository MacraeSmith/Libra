#include "Engine/Renderer/CommandQueue.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Renderer/DX12Utils.hpp"
#include "Engine/Renderer/RendererDX12.hpp"
#include "Engine/Renderer/CommandList.hpp"
#include "Engine/Renderer/ResourceStateTracker.hpp"

#include <cassert>



CommandQueue::CommandQueue(RendererDX12 const* renderer, D3D12_COMMAND_LIST_TYPE type)
	: m_renderer(renderer)
	, m_fenceValue(0)
	, m_commandListType(type)
	, m_bProcessInFlightCommandLists(true)
{
	D3D12_COMMAND_QUEUE_DESC desc = {};
	desc.Type = type;
	desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	desc.NodeMask = 0;

	ID3D12Device* device = m_renderer->GetDevice();
	HRESULT hr = device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_d3d12CommandQueue));
	GUARANTEE_OR_DIE(SUCCEEDED(hr), "could not create Command Queue");

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

	std::string commandQueueName = Stringf("CommandQueue_%s", typeText.c_str());
	SetCommandQueueName(m_d3d12CommandQueue, commandQueueName);

	hr = device->CreateFence(m_fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_d3d12Fence));

	std::string fenceName = Stringf("%s_m_fence", commandQueueName.c_str());
	SetFenceName(m_d3d12Fence, fenceName);
	GUARANTEE_OR_DIE(SUCCEEDED(hr), "could not create Fence");

	m_ProcessInFlightCommandListsThread = std::thread(&CommandQueue::ProcessInFlightCommandLists, this);

	InitializeCommandLists(commandQueueName);
}

CommandQueue::~CommandQueue()
{
	m_bProcessInFlightCommandLists = false;
	m_ProcessInFlightCommandListsThread.join();

	CommandList* commandList = nullptr;
	while (m_AvailableCommandLists.TryPop(commandList))
	{
		delete commandList;
	}
	
	m_renderer = nullptr;
	DX_SAFE_RELEASE(m_d3d12Fence);
	DX_SAFE_RELEASE(m_d3d12CommandQueue);
}

CommandList* CommandQueue::GetCommandList()
{
	CommandList* commandList = nullptr;

	// If there is a command list on the queue.
	if (!m_AvailableCommandLists.Empty())
	{
		m_AvailableCommandLists.TryPop(commandList);
	}

	else
	{
		while (true)
		{
			{
				std::unique_lock<std::mutex> lock(m_ProcessInFlightCommandListsThreadMutex);

				// Try popping again after lock to be safe
				if (m_AvailableCommandLists.TryPop(commandList))
				{
					break; // got one, break the wait loop
				}
			}

			// No command list available, sleep/yield and let ProcessInFlightCommandLists do its job
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}

	return commandList;
}


uint64_t CommandQueue::ExecuteCommandList(CommandList* commandList)
{
	return ExecuteCommandLists(std::vector<CommandList*>({ commandList }));
}

uint64_t CommandQueue::ExecuteCommandLists(std::vector<CommandList*> const& commandLists)
{
	ResourceStateTracker::Lock();

	// Command lists that need to put back on the command list queue.
	std::vector<CommandList* > toBeQueued;
	toBeQueued.reserve(commandLists.size() * 2);        // 2x since each command list will have a pending command list.

	// Generate mips command lists.
	//std::vector<std::shared_ptr<CommandList> > generateMipsCommandLists;
	//generateMipsCommandLists.reserve(commandLists.size());

	// Command lists that need to be executed.
	std::vector<ID3D12CommandList*> d3d12CommandLists;
	d3d12CommandLists.reserve(commandLists.size() * 2); // 2x since each command list will have a pending command list.

	for (int i = 0; i < (int)commandLists.size(); ++i)
	{
		CommandList* commandList = commandLists[i];
		CommandList* pendingCommandList = GetCommandList();
		bool hasPendingBarriers = commandList->Close(*pendingCommandList);
		pendingCommandList->Close();
		// If there are no pending barriers on the pending command list, there is no reason to 
		// execute an empty command list on the command queue.
		if (hasPendingBarriers)
		{
			d3d12CommandLists.push_back(pendingCommandList->GetGraphicsCommandList());
		}
		d3d12CommandLists.push_back(commandList->GetGraphicsCommandList());

		toBeQueued.push_back(pendingCommandList);
		toBeQueued.push_back(commandList);

		//auto generateMipsCommandList = commandList->GetGenerateMipsCommandList();
		//if (generateMipsCommandList)
		{
			//generateMipsCommandLists.push_back(generateMipsCommandList);
		}
	}

	UINT numCommandLists = static_cast<UINT>(d3d12CommandLists.size());
	m_d3d12CommandQueue->ExecuteCommandLists(numCommandLists, d3d12CommandLists.data());
	uint64_t fenceValue = Signal();

	ResourceStateTracker::Unlock();

	// Queue command lists for reuse.
	for (auto commandList : toBeQueued)
	{
		m_InFlightCommandLists.Push({ fenceValue, commandList });
	}

	/*
	// If there are any command lists that generate mips then execute those
	// after the initial resource command lists have finished.
	if (generateMipsCommandLists.size() > 0)
	{
		auto computeQueue = Application::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COMPUTE);
		computeQueue->Wait(*this);
		computeQueue->ExecuteCommandLists(generateMipsCommandLists);
	}
	*/

	return fenceValue;
}

uint64_t CommandQueue::Signal()
{
	uint64_t fenceValue = ++m_fenceValue;
	HRESULT hr = m_d3d12CommandQueue->Signal(m_d3d12Fence, fenceValue);
	GUARANTEE_OR_DIE(SUCCEEDED(hr), "Failed to signal GPU fence");
	return fenceValue;
}

bool CommandQueue::IsFenceComplete(uint64_t fenceValue)
{
	return m_d3d12Fence->GetCompletedValue() >= fenceValue;
}

void CommandQueue::WaitForFenceValue(uint64_t fenceValue)
{
	if (!IsFenceComplete(fenceValue))
	{
		HANDLE event = ::CreateEvent(NULL, FALSE, FALSE, NULL);
		assert(event && "Failed to create fence event handle.");
		
		HRESULT hr = m_d3d12Fence->SetEventOnCompletion(fenceValue, event);
		GUARANTEE_OR_DIE(SUCCEEDED(hr), "Failed to set GPU Fence Event");
		::WaitForSingleObject(event, INFINITE);
		::CloseHandle(event);
	}
}

void CommandQueue::Flush()
{
	std::unique_lock<std::mutex> lock(m_ProcessInFlightCommandListsThreadMutex);
	m_ProcessInFlightCommandListsThreadCV.wait(lock, [this] { return m_InFlightCommandLists.Empty(); });

	WaitForFenceValue(Signal());
}

void CommandQueue::Wait(CommandQueue const& other)
{
	m_d3d12CommandQueue->Wait(other.m_d3d12Fence, other.m_fenceValue);
}

ID3D12CommandQueue* CommandQueue::GetD3D12CommandQueue() const
{
	return m_d3d12CommandQueue;
}


void CommandQueue::ProcessInFlightCommandLists()
{

	while (m_bProcessInFlightCommandLists)
	{
		CommandListEntry commandListEntry;
		bool hasEntry = false;

		{
			std::unique_lock<std::mutex> innerLock(m_ProcessInFlightCommandListsThreadMutex);
			hasEntry = m_InFlightCommandLists.TryPop(commandListEntry);
		}

		if (hasEntry)
		{
			auto fenceValue = std::get<0>(commandListEntry);
			auto commandList = std::get<1>(commandListEntry);

			WaitForFenceValue(fenceValue);     // Wait for GPU
			commandList->Reset();              // Reset command list + allocator
			m_AvailableCommandLists.Push(commandList);

			{
				std::unique_lock<std::mutex> innerLock(m_ProcessInFlightCommandListsThreadMutex);
				m_ProcessInFlightCommandListsThreadCV.notify_one(); // Wake flush
			}
		}
		else
		{
			std::this_thread::yield();
		}
	}

	//vvv older version that does not locks unnecessarily. A lot slower
	/*
	std::unique_lock<std::mutex> lock(m_ProcessInFlightCommandListsThreadMutex, std::defer_lock);

	while (m_bProcessInFlightCommandLists)
	{
		CommandListEntry commandListEntry;

		lock.lock();
		while (m_InFlightCommandLists.TryPop(commandListEntry))
		{
			auto fenceValue = std::get<0>(commandListEntry);
			auto commandList = std::get<1>(commandListEntry);

			WaitForFenceValue(fenceValue);

			commandList->Reset();

			m_AvailableCommandLists.Push(commandList);
		}
		lock.unlock();
		m_ProcessInFlightCommandListsThreadCV.notify_one();

		std::this_thread::yield();
	}
	*/
}

void CommandQueue::InitializeCommandLists(std::string const& name)
{
	for (int i = 0; i < MAX_NUM_COMMAND_LISTS; ++i)
	{
		std::string commandListName = Stringf("%s Command List: %i", name.c_str(), i);
		CommandList* commandList = new CommandList(m_renderer, m_commandListType, commandListName);
		m_AvailableCommandLists.Push(commandList);
	}
}
