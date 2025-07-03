#pragma once
#include <string>

struct ID3D12PipelineState;
class ShaderDX12;
class RootSignatureDX12;

class PipelineStateObjectDX12
{
public:
	explicit PipelineStateObjectDX12(ShaderDX12* shader, RootSignatureDX12* rootSignature, std::string const& name);
	virtual ~PipelineStateObjectDX12();

	std::string GetName() const {return m_name;}

public:
	ID3D12PipelineState* m_pipelineState = nullptr;
	ShaderDX12* m_shader = nullptr;
	RootSignatureDX12* m_rootSignature = nullptr;
	std::string m_name;

};

