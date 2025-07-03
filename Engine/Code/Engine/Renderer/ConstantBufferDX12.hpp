#pragma once
#include "Engine/Renderer/BufferDX12.hpp"
#include "Engine/Renderer/DescriptorAllocation.hpp"
class ConstantBufferDX12 : public BufferDX12
{
	friend class Renderer;
	friend class RendererDX12;

public:
	ConstantBufferDX12(RendererDX12* renderer, std::string const& name = "Added Constant Buffer");
	//ConstantBufferDX12(ID3D12Device2* device, size_t size);
	ConstantBufferDX12(ConstantBufferDX12 const& copy) = delete;
	virtual ~ConstantBufferDX12();

	virtual void CreateViews(size_t numElements, size_t elementSize) override;

	size_t GetSizeInBytes() const
	{
		return m_SizeInBytes;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE GetConstantBufferView() const
	{
		return m_constantBufferView.GetDescriptorHandle();
	}

	virtual D3D12_CPU_DESCRIPTOR_HANDLE GetShaderResourceView(D3D12_SHADER_RESOURCE_VIEW_DESC const* srvDesc = nullptr) const override;
	virtual D3D12_CPU_DESCRIPTOR_HANDLE GetUnorderedAccessView(D3D12_UNORDERED_ACCESS_VIEW_DESC const* uavDesc = nullptr) const override;

private:
	//void Create();
private:
	//ID3D12Resource* m_buffer = nullptr;
	size_t m_SizeInBytes = 0;
	DescriptorAllocation m_constantBufferView;
};

