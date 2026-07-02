#include "render/passes/RenderPass.h"

RenderPass::RenderPass(const std::string& passName) : m_name(passName)
{
}

void RenderPass::AddDependency(const std::string& dependency)
{
	m_dependencies.push_back(dependency);
}

void RenderPass::Execute(vk::raii::CommandBuffer& commandBuffer, const FrameInfo& frame)
{
	if (!m_enabled) return;

	BeginPass(commandBuffer, frame);
	Render(commandBuffer, frame);
	EndPass(commandBuffer, frame);
}
