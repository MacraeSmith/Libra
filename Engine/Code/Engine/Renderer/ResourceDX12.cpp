#include "Engine/Renderer/ResourceDX12.hpp"
#include "Engine/Renderer/RendererDX12.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Renderer/ResourceStateTracker.hpp"

ResourceDX12::ResourceDX12(RendererDX12 const* renderer, std::string const& name)
	:m_renderer(renderer)
	,m_ResourceName(name)
{
}

ResourceDX12::ResourceDX12(RendererDX12 const* renderer, D3D12_RESOURCE_DESC const& resourceDesc, const D3D12_CLEAR_VALUE* clearValue, std::string const& name)
	:m_renderer(renderer)
{
	ID3D12Device* device = m_renderer->GetDevice();

	if (clearValue)
	{
		m_d3d12ClearValue = std::make_unique<D3D12_CLEAR_VALUE>(*clearValue);
	}

	D3D12_HEAP_PROPERTIES heapProperties = {};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProperties.CreationNodeMask = 1;
	heapProperties.VisibleNodeMask = 1;

	HRESULT hr = device->CreateCommittedResource(&heapProperties,D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COMMON, m_d3d12ClearValue.get(), IID_PPV_ARGS(&m_d3d12Resource));
	GUARANTEE_OR_DIE(SUCCEEDED(hr), "could not create commited resource");

	ResourceStateTracker::AddGlobalResourceState(m_d3d12Resource, D3D12_RESOURCE_STATE_COMMON);

	SetName(name);
}

ResourceDX12::ResourceDX12(RendererDX12 const* renderer,ID3D12Resource* resource, std::string const& name)
	: m_renderer(renderer)
	, m_d3d12Resource(resource)
{
	SetName(name);
}

ResourceDX12::ResourceDX12(const ResourceDX12& copy)
	: m_renderer(copy.m_renderer)
	, m_d3d12Resource(copy.m_d3d12Resource)
	, m_ResourceName(copy.m_ResourceName)
	, m_d3d12ClearValue(std::make_unique<D3D12_CLEAR_VALUE>(*copy.m_d3d12ClearValue))
{
}

ResourceDX12::ResourceDX12(ResourceDX12&& copy)
	: m_d3d12Resource(std::move(copy.m_d3d12Resource))
	, m_ResourceName(std::move(copy.m_ResourceName))
	, m_d3d12ClearValue(std::move(copy.m_d3d12ClearValue))
{
}

ResourceDX12& ResourceDX12::operator=(const ResourceDX12& other)
{
	if (this != &other)
	{
		m_d3d12Resource = other.m_d3d12Resource;
		m_ResourceName = other.m_ResourceName;
		if (other.m_d3d12ClearValue)
		{
			m_d3d12ClearValue = std::make_unique<D3D12_CLEAR_VALUE>(*other.m_d3d12ClearValue);
		}
	}

	return *this;
}

ResourceDX12& ResourceDX12::operator=(ResourceDX12&& other)
{
	if (this != &other)
	{
		if (m_d3d12Resource)
		{
			DX_SAFE_RELEASE(m_d3d12Resource);
		}

		m_d3d12Resource = other.m_d3d12Resource;
		m_ResourceName = other.m_ResourceName;
		m_d3d12ClearValue = std::move(other.m_d3d12ClearValue);

		other.m_d3d12Resource = nullptr;
		other.m_ResourceName.clear();
	}

	return *this;
}


ResourceDX12::~ResourceDX12()
{
	DX_SAFE_RELEASE(m_d3d12Resource);
}

void ResourceDX12::SetD3D12Resource(ID3D12Resource* d3d12Resource, const D3D12_CLEAR_VALUE* clearValue)
{
	m_d3d12Resource = d3d12Resource;
	if (m_d3d12ClearValue)
	{
		m_d3d12ClearValue = std::make_unique<D3D12_CLEAR_VALUE>(*clearValue);
	}
	else
	{
		m_d3d12ClearValue.reset();
	}
	SetName(m_ResourceName);
}

void ResourceDX12::SetName(std::string const& name)
{
	m_ResourceName = name;
	if (m_d3d12Resource && !m_ResourceName.empty())
	{
		SetResourceName(m_d3d12Resource, name);
	}
}

void ResourceDX12::Reset()
{
	DX_SAFE_RELEASE(m_d3d12Resource);
	m_d3d12ClearValue.reset();
}