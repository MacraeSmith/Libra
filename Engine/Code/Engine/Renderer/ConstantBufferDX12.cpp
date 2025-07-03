#include "Engine/Renderer/ConstantBufferDX12.hpp"
#include "Engine/Renderer/RendererDX12.hpp"
#include "Engine/Core/EngineCommon.hpp"

ConstantBufferDX12::ConstantBufferDX12(RendererDX12* renderer, std::string const& name)
	:BufferDX12(renderer, name)
{
	m_constantBufferView = renderer->AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

ConstantBufferDX12::~ConstantBufferDX12()
{
}

void ConstantBufferDX12::CreateViews(size_t numElements, size_t elementSize)
{
	m_SizeInBytes = numElements * elementSize;

	D3D12_CONSTANT_BUFFER_VIEW_DESC d3d12ConstantBufferViewDesc;
	d3d12ConstantBufferViewDesc.BufferLocation = m_d3d12Resource->GetGPUVirtualAddress();
	d3d12ConstantBufferViewDesc.SizeInBytes = static_cast<UINT>(AlignUp(m_SizeInBytes, 16));

	ID3D12Device* device = m_renderer->GetDevice();

	device->CreateConstantBufferView(&d3d12ConstantBufferViewDesc, m_constantBufferView.GetDescriptorHandle());
}

D3D12_CPU_DESCRIPTOR_HANDLE ConstantBufferDX12::GetShaderResourceView(D3D12_SHADER_RESOURCE_VIEW_DESC const* srvDesc) const
{
	UNUSED(srvDesc);
	ERROR_AND_DIE("ConstantBuffer::GetShaderResourceView() should not be called");
}

D3D12_CPU_DESCRIPTOR_HANDLE ConstantBufferDX12::GetUnorderedAccessView(D3D12_UNORDERED_ACCESS_VIEW_DESC const* uavDesc) const
{
	UNUSED(uavDesc);
	ERROR_AND_DIE("ConstantBuffer::GetUnorderedAccessView() should not be called");
}

/*
ConstantBufferDX12::ConstantBufferDX12(ID3D12Device2* device, size_t size)
	:m_device(device)
	,m_size(size)
{
	Create();
}

ConstantBufferDX12::~ConstantBufferDX12()
{
	DX_SAFE_RELEASE(m_buffer);
	m_device = nullptr;
}
void ConstantBufferDX12::Create()
{
	D3D12_HEAP_PROPERTIES heapProperties = {};
	heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProperties.CreationNodeMask = 1;
	heapProperties.VisibleNodeMask = 1;


	// 2. Resource description for a buffer of given size
	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Alignment = 0;
	resourceDesc.Width = m_SizeInBytes;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	HRESULT hr = m_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_buffer));
	GUARANTEE_OR_DIE(SUCCEEDED(hr), "could not create committed resource for constant buffer");

	SetResourceName(m_buffer, "ConstantBufferDX12");
}

*/