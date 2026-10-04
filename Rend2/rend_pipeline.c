/* Prepared Vulkan SPIR-V graphics pipeline; shader ABI tag is caller assertion,
 * not reflection. */
#include "rend_internal.h"
#include <string.h>
static VkFormat
rend_pipe_format(RendFormat f)
{
	return f == REND_FORMAT_RGBA8_UNORM   ? VK_FORMAT_R8G8B8A8_UNORM
	       : f == REND_FORMAT_BGRA8_UNORM ? VK_FORMAT_B8G8R8A8_UNORM
	       : f == REND_FORMAT_D32_FLOAT   ? VK_FORMAT_D32_SFLOAT
					      : VK_FORMAT_UNDEFINED;
}
int
rend_graphics_pipeline_create(RendBackend b,
			      const RendGraphicsPipelineDesc *d,
			      RendPipeline *out)
{
	uint32_t i;
	RendPipelineRecord *p = NULL;
	VkShaderModule modules[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE};
	VkShaderModuleCreateInfo sm;
	VkDescriptorSetAndBindingMappingEXT root;
	VkShaderDescriptorSetAndBindingMappingInfoEXT mapping;
	VkPipelineCreateFlags2CreateInfo flags;
	VkPipelineShaderStageCreateInfo stages[2];
	VkPipelineVertexInputStateCreateInfo vi;
	VkPipelineInputAssemblyStateCreateInfo ia;
	VkPipelineViewportStateCreateInfo vp;
	VkPipelineRasterizationStateCreateInfo rs;
	VkPipelineMultisampleStateCreateInfo ms;
	VkPipelineColorBlendAttachmentState ba;
	VkPipelineColorBlendStateCreateInfo blend;
	VkDynamicState ds[5] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR,
				VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE,
				VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE,
				VK_DYNAMIC_STATE_DEPTH_COMPARE_OP};
	VkPipelineDepthStencilStateCreateInfo depth;
	VkPipelineDynamicStateCreateInfo dyn;
	VkPipelineRenderingCreateInfo rendering;
	VkGraphicsPipelineCreateInfo ci;
	VkFormat fmt;
	VkResult r;
	if (out)
		*out = NULL;
	if (!out || !b || b->magic != REND_BACKEND_MAGIC || !d ||
	    d->abi_tag != REND_SHADER_ABI_SLANG_TYPED_POINTER_ROOT_V1 ||
	    !d->vertex.bytes || !d->fragment.bytes || d->vertex.size < 20 ||
	    d->fragment.size < 20 || d->vertex.size % 4 || d->fragment.size % 4 ||
	    ((const uint32_t *)d->vertex.bytes)[0] != 0x07230203 ||
	    ((const uint32_t *)d->fragment.bytes)[0] != 0x07230203 ||
	    !(fmt = rend_pipe_format(d->color_format)) ||
	    d->depth_format != REND_FORMAT_D32_FLOAT || d->samples != 1 ||
	    (uintptr_t)d->vertex.bytes % sizeof(uint32_t) ||
	    (uintptr_t)d->fragment.bytes % sizeof(uint32_t))
	{
		rend_vk_set_diagnostic(
			REND_DIAG_INVALID_ARGUMENT, 0,
			"invalid pipeline request or untagged/non-SPIR-V snake artifact");
		return 0;
	}
	for (i = 0; i < b->pipeline_count; i++)
		if (!b->pipelines[i].live)
		{
			p = &b->pipelines[i];
			break;
		}
	if (!p)
	{
		rend_vk_set_diagnostic(REND_DIAG_CAPACITY, 0,
				       "pipeline record capacity exhausted");
		return 0;
	}
	memset(&sm, 0, sizeof(sm));
	sm.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	sm.codeSize = d->vertex.size;
	sm.pCode = d->vertex.bytes;
	r = vkCreateShaderModule(b->device, &sm, NULL, &modules[0]);
	if (r != VK_SUCCESS)
		goto fail;
	sm.codeSize = d->fragment.size;
	sm.pCode = d->fragment.bytes;
	r = vkCreateShaderModule(b->device, &sm, NULL, &modules[1]);
	if (r != VK_SUCCESS)
		goto fail;
	memset(&root, 0, sizeof(root));
	root.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_AND_BINDING_MAPPING_EXT;
	root.descriptorSet = 0;
	root.firstBinding = 0;
	root.bindingCount = 1;
	root.resourceMask = VK_SPIRV_RESOURCE_TYPE_UNIFORM_BUFFER_BIT_EXT;
	root.source = VK_DESCRIPTOR_MAPPING_SOURCE_PUSH_ADDRESS_EXT;
	root.sourceData.pushAddressOffset = 0;
	memset(&mapping, 0, sizeof(mapping));
	mapping.sType =
		VK_STRUCTURE_TYPE_SHADER_DESCRIPTOR_SET_AND_BINDING_MAPPING_INFO_EXT;
	mapping.mappingCount = 1;
	mapping.pMappings = &root;
	memset(stages, 0, sizeof(stages));
	stages[0].sType = stages[1].sType =
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].pNext = stages[1].pNext = &mapping;
	stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module = modules[0];
	stages[0].pName = "vertMain";
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module = modules[1];
	stages[1].pName = "fragMain";
	memset(&flags, 0, sizeof(flags));
	flags.sType = VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO;
	flags.flags = VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT;
	memset(&vi, 0, sizeof(vi));
	vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	memset(&ia, 0, sizeof(ia));
	ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	memset(&vp, 0, sizeof(vp));
	vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	vp.viewportCount = 1;
	vp.scissorCount = 1;
	memset(&rs, 0, sizeof(rs));
	rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rs.polygonMode = VK_POLYGON_MODE_FILL;
	rs.cullMode = VK_CULL_MODE_NONE;
	rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rs.lineWidth = 1;
	memset(&ms, 0, sizeof(ms));
	ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	memset(&ba, 0, sizeof(ba));
	ba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
			    VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	memset(&blend, 0, sizeof(blend));
	blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend.attachmentCount = 1;
	blend.pAttachments = &ba;
	memset(&dyn, 0, sizeof(dyn));
	dyn.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dyn.dynamicStateCount = 5;
	dyn.pDynamicStates = ds;
	memset(&depth, 0, sizeof(depth));
	depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	memset(&rendering, 0, sizeof(rendering));
	rendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	rendering.colorAttachmentCount = 1;
	rendering.pColorAttachmentFormats = &fmt;
	rendering.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;
	memset(&ci, 0, sizeof(ci));
	ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	ci.pNext = &flags;
	ci.stageCount = 2;
	ci.pStages = stages;
	ci.pVertexInputState = &vi;
	ci.pInputAssemblyState = &ia;
	ci.pViewportState = &vp;
	ci.pRasterizationState = &rs;
	ci.pMultisampleState = &ms;
	ci.pDepthStencilState = &depth;
	ci.pColorBlendState = &blend;
	ci.pDynamicState = &dyn;
	flags.pNext = &rendering;
	r = vkCreateGraphicsPipelines(b->device, VK_NULL_HANDLE, 1, &ci, NULL,
				      &p->pipeline);
	vkDestroyShaderModule(b->device, modules[0], NULL);
	modules[0] = VK_NULL_HANDLE;
	vkDestroyShaderModule(b->device, modules[1], NULL);
	modules[1] = VK_NULL_HANDLE;
	if (r != VK_SUCCESS)
		goto fail;
	p->backend = b;
	p->format = d->color_format;
	p->depth_format = d->depth_format;
	p->live = 1;
	*out = p;
	return 1;
fail:
	if (modules[0])
		vkDestroyShaderModule(b->device, modules[0], NULL);
	if (modules[1])
		vkDestroyShaderModule(b->device, modules[1], NULL);
	rend_vk_set_diagnostic(REND_DIAG_NATIVE, r,
			       "shader module or native graphics pipeline creation "
			       "failed (entry points vertMain/fragMain)");
	return 0;
}
int
rend_pipeline_destroy(RendBackend b, RendPipeline p)
{
	if (!b || b->magic != REND_BACKEND_MAGIC || !p || p->backend != b ||
	    !p->live || p->refs)
		return 0;
	vkDestroyPipeline(b->device, p->pipeline, NULL);
	memset(p, 0, sizeof(*p));
	return 1;
}
