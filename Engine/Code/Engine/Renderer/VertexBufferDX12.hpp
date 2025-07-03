#pragma once
#include "Engine/Renderer/BufferDX12.hpp"
#include <d3d12.h>


class RendererDX12;
class VertexBufferDX12 : public BufferDX12
{
	friend class RendererDX12;
public:

	VertexBufferDX12(RendererDX12 const* renderer, std::string const& name = "Added Vertex Buffer");
	VertexBufferDX12(VertexBufferDX12 const& copy) = delete;
	virtual ~VertexBufferDX12();

	unsigned int GetStride() const;
	unsigned int GetNumberVerts() const;

	virtual void CreateViews(size_t numElements, size_t elementSize) override;

	D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView() const {return m_vertexBufferView;}

	size_t GetNumVertices() const {return m_numVertices;}

	size_t GetVertexStride() const {return m_stride;}


	virtual D3D12_CPU_DESCRIPTOR_HANDLE GetShaderResourceView(D3D12_SHADER_RESOURCE_VIEW_DESC const* srvDesc = nullptr) const override;
	virtual D3D12_CPU_DESCRIPTOR_HANDLE GetUnorderedAccessView(D3D12_UNORDERED_ACCESS_VIEW_DESC const* uavDesc = nullptr) const override;

public:
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView = {};
	unsigned int m_stride = 0;
	unsigned int m_numVertices = 0;

private:
};

