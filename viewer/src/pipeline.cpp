#include "pipeline.h"
#include <xev/common.h>
#include <xev/logger.h>

namespace viewer {

Pipeline::Pipeline(xev::PipelineManager& pipelineManager,
                   xev::ResourceManager& resourceManager,
                   uint32_t numFramesInFlight)
    : m_pipelineManager(pipelineManager), m_resourceManager(resourceManager) {
  this->info.colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
  this->info.shaderVertSrc = "raster.spv";
  this->info.shaderFragSrc = "raster.spv";
  this->info.pushConstSize = sizeof(PushConstants);
  this->info.enableBlending = true;
  this->info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  this->info.polygonMode = VK_POLYGON_MODE_FILL;
  this->info.cullMode = VK_CULL_MODE_NONE;
  this->info.multisampleCount = VK_SAMPLE_COUNT_1_BIT;
  this->info.enableDepth = false;

  m_pipelineManager.create(*this);

  for (uint32_t i = 0; i < numFramesInFlight; ++i) {
    xev::Buffer infoBuf{
        MAX_QUADS * sizeof(DrawInfo),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_AUTO};
    m_resourceManager.alloc(infoBuf);
    m_drawInfoBuffers.push_back(infoBuf);

    xev::Buffer satBuf{
        MAX_SATELLITES * sizeof(Satellite),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_AUTO};
    m_resourceManager.alloc(satBuf);
    m_satelliteBuffers.push_back(satBuf);
  }
}

Pipeline::~Pipeline() {
  m_pipelineManager.destroy(*this);
  for (auto& buf : m_drawInfoBuffers) {
    m_resourceManager.free(buf);
  }
  for (auto& buf : m_satelliteBuffers) {
    m_resourceManager.free(buf);
  }
}

void Pipeline::update_draw_infos(const std::vector<DrawInfo>& infos, uint32_t frameIdx) {
  m_numQuads = static_cast<uint32_t>(infos.size());
  if (m_numQuads > 0 && frameIdx < m_drawInfoBuffers.size()) {
    m_drawInfoBuffers[frameIdx].write(infos.data(), infos.size() * sizeof(DrawInfo));
  }
}

void Pipeline::update_satellites(const std::vector<Satellite>& satellites, uint32_t frameIdx) {
  m_numSatellites = static_cast<uint32_t>(satellites.size());
  if (m_numSatellites > 0 && frameIdx < m_satelliteBuffers.size()) {
    m_satelliteBuffers[frameIdx].write(satellites.data(), satellites.size() * sizeof(Satellite));
  }
}

void Pipeline::begin_render(VkCommandBuffer cmd, const xev::Image& colorImage) {
  VkRenderingAttachmentInfo colorAttachment = {
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView = colorImage.view,
      .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue = {.color = {.float32 = {0.05f, 0.05f, 0.07f, 1.0f}}},
  };

  VkRenderingInfo renderingInfo = {
      .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
      .renderArea = {{0, 0}, {colorImage.width, colorImage.height}},
      .layerCount = 1,
      .colorAttachmentCount = 1,
      .pColorAttachments = &colorAttachment,
  };

  vkCmdBeginRendering(cmd, &renderingInfo);
}

void Pipeline::draw(VkCommandBuffer cmd,
                    const xev::Image& colorImage,
                    const xev::GlobalDescriptorSet& descSet,
                    PushConstants pc,
                    uint32_t currFrameIdx) {
  XEV_ASSERT(colorImage.on_device());

  descSet.bind(cmd, layout);
  prepare_attachments(cmd, colorImage);

  pc.infoBuffer = m_drawInfoBuffers[currFrameIdx].addr;
  pc.satelliteBuffer = m_satelliteBuffers[currFrameIdx].addr;
  pc.numSatellites = m_numSatellites;

  begin_render(cmd, colorImage);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

  VkViewport viewport = {
      .x = 0.0f,
      .y = 0.0f,
      .width = static_cast<float>(colorImage.width),
      .height = static_cast<float>(colorImage.height),
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
  };
  vkCmdSetViewport(cmd, 0, 1, &viewport);

  VkRect2D scissor = {
      .offset = {0, 0},
      .extent = {colorImage.width, colorImage.height},
  };
  vkCmdSetScissor(cmd, 0, 1, &scissor);

  vkCmdPushConstants(cmd, layout,
                     VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                     0, sizeof(PushConstants), &pc);

  vkCmdDraw(cmd, 3, m_numQuads, 0, 0);
  vkCmdEndRendering(cmd);

  prepare_transfer(cmd, colorImage);
}

} // namespace viewer
