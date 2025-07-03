#pragma once
#include "Engine/Renderer/Shader.hpp"
#include "Engine/Renderer/RendererDX12.hpp"
#include <string>
#include <d3d12.h>


class ShaderDX12
{
	friend class Renderer;
	friend class RendererDX12;
public:
	ShaderDX12(ShaderConfig const& config);
	ShaderDX12(ShaderDX12 const& copy) = delete;
	virtual ~ShaderDX12();

	std::string const& GetName() const;

private:
	ShaderConfig m_config;
	ID3DBlob* m_vertexShaderBlob = nullptr;
	ID3DBlob* m_pixelShaderBlob = nullptr;
	VertexType m_vertexType = VertexType::VERTEX_PCU;


};

