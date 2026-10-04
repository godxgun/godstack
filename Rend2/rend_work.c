/* Direct Vulkan work recording and explicit completion. */
#include "rend_internal.h"
#include <math.h>
#include <string.h>
enum
{
	REND_REF_VIEW = 1,
	REND_REF_TEXTURE = 2,
	REND_REF_PIPELINE = 3,
	REND_REF_HEAP = 4
};
static int
rend_ref(RendCommandList c, uint32_t kind, void *object)
{
	uint32_t i;
	if (!object)
		return 0;
	for (i = 0; i < c->ref_count; i++)
		if (c->refs[i].kind == kind && c->refs[i].object == object)
			return 1;
	if (c->ref_count >= 16)
	{
		c->state = 4;
		rend_vk_set_diagnostic(REND_DIAG_CAPACITY, 0,
				       "command list native-object reference capacity (16) "
				       "exhausted; reset to recover");
		return 0;
	}
	c->refs[c->ref_count].kind = kind;
	c->refs[c->ref_count++].object = object;
	switch (kind)
	{
	case REND_REF_VIEW:
		((RendTextureViewRecord *)object)->refs++;
		break;
	case REND_REF_TEXTURE:
		((RendTextureRecord *)object)->refs++;
		break;
	case REND_REF_PIPELINE:
		((RendPipelineRecord *)object)->refs++;
		break;
	case REND_REF_HEAP:
		((RendHeapRecord *)object)->refs++;
		break;
	}
	return 1;
}
static void
rend_unref_all(RendCommandRecord *c)
{
	uint32_t i;
	for (i = 0; i < c->ref_count; i++)
	{
		void *o = c->refs[i].object;
		switch (c->refs[i].kind)
		{
		case REND_REF_VIEW:
			if (((RendTextureViewRecord *)o)->refs)
				((RendTextureViewRecord *)o)->refs--;
			break;
		case REND_REF_TEXTURE:
			if (((RendTextureRecord *)o)->refs)
				((RendTextureRecord *)o)->refs--;
			break;
		case REND_REF_PIPELINE:
			if (((RendPipelineRecord *)o)->refs)
				((RendPipelineRecord *)o)->refs--;
			break;
		case REND_REF_HEAP:
			if (((RendHeapRecord *)o)->refs)
				((RendHeapRecord *)o)->refs--;
			break;
		}
	}
	c->ref_count = 0;
}
static int
rend_command_fail(RendCommandList c)
{
	if (c && c->state == 1)
		c->state = 4;
	return 0;
}
static int
rend_list_recording(RendCommandList c)
{
	return c && c->backend && c->backend->magic == REND_BACKEND_MAGIC &&
	       !c->backend->terminal && c->state == 1;
}
static void
rend_queue_reap(RendQueueRecord *q)
{
	uint32_t i;
	RendBackend b = q->backend;
	for (i = 0; i < q->submission_capacity; i++)
	{
		RendQueueSubmission *s = &q->submissions[i];
		uint64_t done = 0;
		if (s->live && !s->uncertain &&
		    vkGetSemaphoreCounterValue(b->device, s->timeline->semaphore, &done) ==
			    VK_SUCCESS &&
		    done >= s->value)
		{
			memset(s, 0, sizeof(*s));
			if (q->active_submissions)
				q->active_submissions--;
		}
	}
}
static VkPipelineStageFlags2
rend_vk_stage(uint32_t s)
{
	VkPipelineStageFlags2 r = 0;
	if (s & REND_STAGE_HOST)
		r |= VK_PIPELINE_STAGE_2_HOST_BIT;
	if (s & REND_STAGE_TRANSFER)
		r |= VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
	if (s & REND_STAGE_COMPUTE)
		r |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	if (s & REND_STAGE_INDEX_INPUT)
		r |= VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT;
	if (s & REND_STAGE_VERTEX)
		r |= VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
	if (s & REND_STAGE_TASK)
		r |= VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_EXT;
	if (s & REND_STAGE_MESH)
		r |= VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT;
	if (s & REND_STAGE_FRAGMENT)
		r |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	if (s & REND_STAGE_EARLY_DEPTH_STENCIL)
		r |= VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
	if (s & REND_STAGE_LATE_DEPTH_STENCIL)
		r |= VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
	if (s & REND_STAGE_COLOR_OUTPUT)
		r |= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	if (s & REND_STAGE_INDIRECT)
		r |= VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
	if (s & REND_STAGE_GRAPHICS)
		r |= VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
	return r;
}
static VkAccessFlags2
rend_vk_access(uint32_t a)
{
	return (a & REND_ACCESS_READ ? VK_ACCESS_2_MEMORY_READ_BIT : 0) |
	       (a & REND_ACCESS_WRITE ? VK_ACCESS_2_MEMORY_WRITE_BIT : 0);
}
int
rend_queue_get(RendBackend b, uint32_t i, RendQueue *o)
{
	if (o)
		*o = NULL;
	if (!o || !b || b->magic != REND_BACKEND_MAGIC || i >= b->queue_count)
		return 0;
	*o = &b->queues[i];
	return 1;
}
int
rend_command_pool_get(RendBackend b, uint32_t i, RendCommandPool *o)
{
	if (o)
		*o = NULL;
	if (!o || !b || b->magic != REND_BACKEND_MAGIC || i >= b->pool_count)
		return 0;
	*o = &b->pools[i];
	return 1;
}
int
rend_command_list_begin(RendCommandPool p, uint32_t slot,
			RendCommandList *o)
{
	VkCommandBufferBeginInfo bi;
	VkResult r;
	if (o)
		*o = NULL;
	if (!o || !p || !p->live || p->backend->terminal || slot >= p->count ||
	    p->commands[slot].state)
		return 0;
	memset(&bi, 0, sizeof(bi));
	bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	r = vkBeginCommandBuffer(p->commands[slot].command, &bi);
	if (r != VK_SUCCESS)
	{
		rend_vk_set_diagnostic(REND_DIAG_NATIVE, r, "vkBeginCommandBuffer failed");
		return 0;
	}
	p->commands[slot].state = 1;
	p->commands[slot].in_render = 0;
	p->commands[slot].pipeline = NULL;
	p->commands[slot].viewport_set = 0;
	p->commands[slot].scissor_set = 0;
	p->commands[slot].depth_state_set = 0;
	p->commands[slot].presentation = NULL;
	p->commands[slot].has_depth = 0;
	p->commands[slot].render_width = p->commands[slot].render_height = 0;
	*o = &p->commands[slot];
	return 1;
}
int
rend_command_list_end(RendCommandList c)
{
	VkResult r;
	if (!rend_list_recording(c) || c->in_render)
	{
		if (c && c->state == 1)
			c->state = 4;
		return 0;
	}
	r = vkEndCommandBuffer(c->command);
	if (r != VK_SUCCESS)
	{
		c->state = 4;
		rend_vk_set_diagnostic(REND_DIAG_NATIVE, r,
				       "vkEndCommandBuffer failed; reset pool to recover");
		return 0;
	}
	c->state = 2;
	return 1;
}
int
rend_command_pool_reset(RendCommandPool p)
{
	uint32_t i;
	VkResult r;
	if (!p || !p->live)
		return 0;
	for (i = 0; i < p->count; i++)
		if (p->commands[i].state == 3 && !p->backend->drained)
		{
			uint64_t v = 0;
			if (!p->commands[i].completion ||
			    vkGetSemaphoreCounterValue(p->backend->device,
						       p->commands[i].completion->semaphore,
						       &v) != VK_SUCCESS ||
			    v < p->commands[i].completion_value)
			{
				rend_vk_set_diagnostic(REND_DIAG_INVALID_ARGUMENT, 0,
						       "command pool still has pending GPU use");
				return 0;
			}
		}
	r = vkResetCommandPool(p->backend->device, p->pool, 0);
	if (r != VK_SUCCESS)
	{
		rend_vk_set_diagnostic(REND_DIAG_NATIVE, r, "vkResetCommandPool failed");
		return 0;
	}
	for (i = 0; i < p->count; i++)
	{
		RendCommandRecord *c = &p->commands[i];
		rend_unref_all(c);
		c->state = 0;
		c->completion = NULL;
		c->completion_value = 0;
		c->in_render = 0;
		c->pipeline = NULL;
		c->color_target = c->depth_target = NULL;
		c->presentation = NULL;
		c->has_depth = 0;
		c->viewport_set = c->scissor_set = c->depth_state_set = 0;
		c->render_width = c->render_height = 0;
		c->active_color_format = REND_FORMAT_NONE;
	}
	return 1;
}
int
rend_cmd_barrier(RendCommandList c, const RendBarrier *b)
{
	const uint32_t all =
		REND_STAGE_HOST | REND_STAGE_TRANSFER | REND_STAGE_COMPUTE |
		REND_STAGE_INDEX_INPUT | REND_STAGE_VERTEX | REND_STAGE_FRAGMENT |
		REND_STAGE_EARLY_DEPTH_STENCIL | REND_STAGE_LATE_DEPTH_STENCIL |
		REND_STAGE_COLOR_OUTPUT | REND_STAGE_INDIRECT | REND_STAGE_GRAPHICS;
	VkMemoryBarrier2 m;
	VkDependencyInfo d;
	if (!rend_list_recording(c) || !b || c->in_render ||
	    (b->producer_stages & ~all) || (b->consumer_stages & ~all) ||
	    (b->producer_access & ~(REND_ACCESS_READ | REND_ACCESS_WRITE)) ||
	    (b->consumer_access & ~(REND_ACCESS_READ | REND_ACCESS_WRITE)) ||
	    (!b->producer_stages && b->producer_access) ||
	    (!b->consumer_stages && b->consumer_access) ||
	    ((b->producer_stages & (REND_STAGE_INDEX_INPUT | REND_STAGE_INDIRECT)) &&
	     (b->producer_access & REND_ACCESS_WRITE)) ||
	    ((b->consumer_stages & (REND_STAGE_INDEX_INPUT | REND_STAGE_INDIRECT)) &&
	     (b->consumer_access & REND_ACCESS_WRITE)))
		return rend_command_fail(c);
	memset(&m, 0, sizeof(m));
	m.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
	m.srcStageMask = rend_vk_stage(b->producer_stages);
	m.srcAccessMask = rend_vk_access(b->producer_access);
	m.dstStageMask = rend_vk_stage(b->consumer_stages);
	m.dstAccessMask = rend_vk_access(b->consumer_access);
	memset(&d, 0, sizeof(d));
	d.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	d.memoryBarrierCount = 1;
	d.pMemoryBarriers = &m;
	vkCmdPipelineBarrier2(c->command, &d);
	return 1;
}
static void
rend_swap_transition(RendCommandRecord *c,
		     RendPresentationRecord *p,
		     VkImageLayout next)
{
	VkImageMemoryBarrier2 m;
	VkDependencyInfo d;
	memset(&m, 0, sizeof(m));
	m.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	m.srcStageMask = next == VK_IMAGE_LAYOUT_GENERAL
				 ? VK_PIPELINE_STAGE_2_NONE
				 : VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	m.srcAccessMask = next == VK_IMAGE_LAYOUT_GENERAL
				  ? 0
				  : VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
	m.dstStageMask = next == VK_IMAGE_LAYOUT_GENERAL
				 ? VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT
				 : VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
	m.dstAccessMask = next == VK_IMAGE_LAYOUT_GENERAL
				  ? VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
				  : 0;
	m.oldLayout = next == VK_IMAGE_LAYOUT_GENERAL ? VK_IMAGE_LAYOUT_UNDEFINED
						      : VK_IMAGE_LAYOUT_GENERAL;
	m.newLayout = next;
	m.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	m.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	m.image = p->images[p->image_index];
	m.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	m.subresourceRange.levelCount = 1;
	m.subresourceRange.layerCount = 1;
	memset(&d, 0, sizeof(d));
	d.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	d.imageMemoryBarrierCount = 1;
	d.pImageMemoryBarriers = &m;
	vkCmdPipelineBarrier2(c->command, &d);
}
static int
rend_activate(RendCommandList c, RendTextureRecord *t)
{
	(void)c;
	{
		VkImageMemoryBarrier2 m;
		VkDependencyInfo d;
		memset(&m, 0, sizeof(m));
		m.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		m.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
		m.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		m.dstAccessMask =
			VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
		m.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		m.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		m.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		m.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		m.image = t->image;
		m.subresourceRange.aspectMask = t->desc.format == REND_FORMAT_D32_FLOAT
							? VK_IMAGE_ASPECT_DEPTH_BIT
							: VK_IMAGE_ASPECT_COLOR_BIT;
		m.subresourceRange.levelCount = t->desc.mip_levels;
		m.subresourceRange.layerCount = t->desc.array_layers;
		memset(&d, 0, sizeof(d));
		d.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		d.imageMemoryBarrierCount = 1;
		d.pImageMemoryBarriers = &m;
		vkCmdPipelineBarrier2(c->command, &d);
	}
	return 1;
}
int
rend_cmd_begin_render(RendCommandList c, const RendRenderDesc *x)
{
	VkRenderingAttachmentInfo colors[1], depth;
	VkRenderingInfo info;
	uint32_t i;
	if (!rend_list_recording(c) || c->in_render || !x || !x->width ||
	    !x->height || !x->colors || x->color_count != 1 || !x->depth_attachment)
		return rend_command_fail(c);
	c->has_depth = 0;
	c->depth_target = NULL;
	c->color_target = NULL;
	c->viewport_set = c->scissor_set = c->depth_state_set = 0;
	for (i = 0; i < x->color_count; i++)
	{
		RendTextureViewRecord *v = x->colors[i].view;
		if (!v || !v->live || v->backend != c->backend ||
		    v->desc.aspect != REND_ASPECT_COLOR || x->width > v->width ||
		    x->height > v->height ||
		    (!v->external &&
		     !(v->texture->desc.usage & REND_TEXTURE_COLOR_ATTACHMENT)) ||
		    !rend_ref(c, REND_REF_VIEW, v))
			return rend_command_fail(c);
		if (v->external)
		{
			if (!v->presentation || !v->presentation->acquired ||
			    (c->presentation && c->presentation != v->presentation))
				return rend_command_fail(c);
			c->presentation = v->presentation;
			rend_swap_transition(c, v->presentation, VK_IMAGE_LAYOUT_GENERAL);
		} else
		{
			rend_activate(c, v->texture);
			if (!rend_ref(c, REND_REF_TEXTURE, v->texture))
				return rend_command_fail(c);
		}
		memset(&colors[i], 0, sizeof(colors[i]));
		colors[i].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		colors[i].imageView = v->view;
		colors[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		colors[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colors[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		for (uint32_t channel = 0; channel < 4; channel++)
			if (!isfinite(x->colors[i].clear[channel]))
				return rend_command_fail(c);
		memcpy(colors[i].clearValue.color.float32, x->colors[i].clear,
		       sizeof(x->colors[i].clear));
	}
	if (x->depth_attachment)
	{
		RendTextureViewRecord *v = x->depth_attachment->view;
		if (!v || !v->live || v->backend != c->backend ||
		    v->desc.aspect != REND_ASPECT_DEPTH || v->external ||
		    v->format != REND_FORMAT_D32_FLOAT ||
		    !(v->texture->desc.usage & REND_TEXTURE_DEPTH_ATTACHMENT) ||
		    x->width > v->width || x->height > v->height ||
		    !rend_ref(c, REND_REF_VIEW, v) ||
		    !rend_ref(c, REND_REF_TEXTURE, v->texture))
			return rend_command_fail(c);
		rend_activate(c, v->texture);
		memset(&depth, 0, sizeof(depth));
		depth.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		depth.imageView = v->view;
		depth.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		if (!isfinite(x->depth_attachment->clear_depth) ||
		    x->depth_attachment->clear_depth < 0.0f ||
		    x->depth_attachment->clear_depth > 1.0f)
			return rend_command_fail(c);
		depth.clearValue.depthStencil.depth = x->depth_attachment->clear_depth;
		c->depth_target = v->texture;
		c->has_depth = 1;
	}
	memset(&info, 0, sizeof(info));
	info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	info.renderArea.extent.width = x->width;
	info.renderArea.extent.height = x->height;
	info.layerCount = 1;
	info.colorAttachmentCount = x->color_count;
	info.pColorAttachments = colors;
	info.pDepthAttachment = x->depth_attachment ? &depth : NULL;
	vkCmdBeginRendering(c->command, &info);
	c->in_render = 1;
	c->color_target = x->colors[0].view->texture;
	c->active_color_format = x->colors[0].view->format;
	c->render_width = x->width;
	c->render_height = x->height;
	return 1;
}
int
rend_cmd_end_render(RendCommandList c)
{
	if (!rend_list_recording(c) || !c->in_render)
		return rend_command_fail(c);
	vkCmdEndRendering(c->command);
	if (c->presentation)
		rend_swap_transition(c, c->presentation, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
	c->in_render = 0;
	return 1;
}
int
rend_cmd_set_pipeline(RendCommandList c, RendPipeline p)
{
	if (!rend_list_recording(c) || !c->in_render || !p || !p->live ||
	    p->backend != c->backend || p->format != c->active_color_format ||
	    !c->has_depth || p->depth_format != REND_FORMAT_D32_FLOAT ||
	    !rend_ref(c, REND_REF_PIPELINE, p))
		return rend_command_fail(c);
	vkCmdBindPipeline(c->command, VK_PIPELINE_BIND_POINT_GRAPHICS, p->pipeline);
	c->pipeline = p;
	return 1;
}
int
rend_cmd_set_viewport(RendCommandList c, const RendViewport *v)
{
	VkViewport n;
	if (!rend_list_recording(c) || !c->in_render || !v || !isfinite(v->x) ||
	    !isfinite(v->y) || !isfinite(v->width) || !isfinite(v->height) ||
	    !isfinite(v->min_depth) || !isfinite(v->max_depth) || v->width <= 0 ||
	    v->height <= 0 || v->x < 0 || v->y < 0 ||
	    v->x + v->width > c->render_width ||
	    v->y + v->height > c->render_height || v->min_depth < 0 ||
	    v->max_depth > 1 || v->min_depth > v->max_depth)
		return rend_command_fail(c);
	n = (VkViewport){v->x, v->y, v->width, v->height, v->min_depth, v->max_depth};
	vkCmdSetViewport(c->command, 0, 1, &n);
	c->viewport_set = 1;
	return 1;
}
int
rend_cmd_set_scissor(RendCommandList c, const RendScissor *s)
{
	VkRect2D n;
	if (!rend_list_recording(c) || !c->in_render || !s || s->x < 0 || s->y < 0 ||
	    !s->width || !s->height || (uint64_t)s->x + s->width > c->render_width ||
	    (uint64_t)s->y + s->height > c->render_height)
		return rend_command_fail(c);
	n.offset.x = s->x;
	n.offset.y = s->y;
	n.extent.width = s->width;
	n.extent.height = s->height;
	vkCmdSetScissor(c->command, 0, 1, &n);
	c->scissor_set = 1;
	return 1;
}
static VkCompareOp
rend_compare(RendCompareOp c)
{
	if (c == REND_COMPARE_NONE || c == REND_COMPARE_ALWAYS)
		return VK_COMPARE_OP_ALWAYS;
	if (c == REND_COMPARE_LESS)
		return VK_COMPARE_OP_LESS;
	return VK_COMPARE_OP_MAX_ENUM;
}
int
rend_cmd_set_depth_state(RendCommandList c, const RendDepthState *s)
{
	VkCompareOp op;
	if (!rend_list_recording(c) || !c->in_render || !s || s->test_enable > 1 ||
	    s->write_enable > 1 ||
	    (op = rend_compare(s->compare)) == VK_COMPARE_OP_MAX_ENUM ||
	    (s->compare == REND_COMPARE_NONE && s->test_enable))
		return rend_command_fail(c);
	vkCmdSetDepthTestEnable(c->command, s->test_enable);
	vkCmdSetDepthWriteEnable(c->command, s->write_enable);
	vkCmdSetDepthCompareOp(c->command, op);
	c->depth_state_set = 1;
	return 1;
}
int
rend_cmd_draw_indexed(RendCommandList c, uint64_t root, uint64_t index,
		      RendIndexType type, uint32_t count,
		      uint32_t instances, uint32_t first, int32_t offset,
		      uint32_t first_instance)
{
	VkBindIndexBuffer3InfoKHR bind;
	VkPushDataInfoEXT push;
	VkDeviceAddress address = root;
	VkIndexType index_type;
	uint32_t width;
	if (!rend_list_recording(c) || !c->in_render || !c->pipeline || !root ||
	    root % c->backend->root_alignment || !index || !count || !instances ||
	    !c->viewport_set || !c->scissor_set || !c->depth_state_set ||
	    (type != REND_INDEX_UINT16 && type != REND_INDEX_UINT32) ||
	    first > UINT32_MAX - count)
		return rend_command_fail(c);
	width = type == REND_INDEX_UINT16 ? 2 : 4;
	if (index % width ||
	    (uint64_t)first + count > (UINT64_MAX - index) / width)
		return rend_command_fail(c);
	index_type =
		type == REND_INDEX_UINT16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
	memset(&bind, 0, sizeof(bind));
	bind.sType = VK_STRUCTURE_TYPE_BIND_INDEX_BUFFER_3_INFO_KHR;
	bind.addressRange.address = index + (VkDeviceAddress)first * width;
	bind.addressRange.size = (VkDeviceSize)count * width;
	bind.addressFlags = VK_ADDRESS_COMMAND_FULLY_BOUND_BIT_KHR | VK_ADDRESS_COMMAND_STORAGE_BUFFER_USAGE_BIT_KHR;
	bind.indexType = index_type;
	c->backend->bind_index(c->command, &bind);
	memset(&push, 0, sizeof(push));
	push.sType = VK_STRUCTURE_TYPE_PUSH_DATA_INFO_EXT;
	push.data.address = &address;
	push.data.size = sizeof(address);
	c->backend->push_data(c->command, &push);
	vkCmdDrawIndexed(c->command, count, instances, 0, offset, first_instance);
	return 1;
}
int
rend_cmd_copy_texture_to_data(RendCommandList c, RendTexture t,
			      RendHeapRange dst, uint32_t width,
			      uint32_t height)
{
	VkDeviceMemoryImageCopyKHR region;
	VkCopyDeviceMemoryImageInfoKHR copy;
	RendHeapRecord *h = dst.heap;
	uint64_t bytes;
	if (!rend_list_recording(c) || c->in_render || !t || !t->live ||
	    t->backend != c->backend || !h || h->backend != c->backend || !h->live ||
	    h->kind != REND_HEAP_DATA || t->desc.format != REND_FORMAT_RGBA8_UNORM ||
	    !(t->desc.usage & REND_TEXTURE_TRANSFER_SRC) || !width || !height ||
	    width > t->desc.width || height > t->desc.height || dst.offset % 4 ||
	    width > UINT64_MAX / height / 4)
		return rend_command_fail(c);
	bytes = (uint64_t)width * height * 4;
	if (dst.offset > h->size || bytes > h->size - dst.offset ||
	    dst.size < bytes || !h->address ||
	    dst.offset > UINT64_MAX - h->address ||
	    bytes > UINT64_MAX - h->address - dst.offset ||
	    !rend_ref(c, REND_REF_TEXTURE, t) ||
	    !rend_ref(c, REND_REF_HEAP, h))
		return rend_command_fail(c);
	memset(&region, 0, sizeof(region));
	region.sType = VK_STRUCTURE_TYPE_DEVICE_MEMORY_IMAGE_COPY_KHR;
	region.addressRange.address = h->address + dst.offset;
	region.addressRange.size = bytes;
	region.addressFlags = VK_ADDRESS_COMMAND_FULLY_BOUND_BIT_KHR |
			      VK_ADDRESS_COMMAND_STORAGE_BUFFER_USAGE_BIT_KHR;
	region.addressRowLength = width;
	region.addressImageHeight = height;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.layerCount = 1;
	region.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	region.imageExtent.width = width;
	region.imageExtent.height = height;
	region.imageExtent.depth = 1;
	memset(&copy, 0, sizeof(copy));
	copy.sType = VK_STRUCTURE_TYPE_COPY_DEVICE_MEMORY_IMAGE_INFO_KHR;
	copy.image = t->image;
	copy.regionCount = 1;
	copy.pRegions = &region;
	c->backend->copy_from_image(c->command, &copy);
	return 1;
}
int
rend_timeline_create(RendQueue q, uint64_t initial, RendTimeline *out)
{
	uint32_t i;
	RendTimelineRecord *t = NULL;
	VkSemaphoreTypeCreateInfo ti;
	VkSemaphoreCreateInfo ci;
	VkResult r;
	if (out)
		*out = NULL;
	if (!out || !q || !q->live || q->backend->terminal)
		return 0;
	for (i = 0; i < q->backend->timeline_count; i++)
		if (!q->backend->timelines[i].live)
		{
			t = &q->backend->timelines[i];
			break;
		}
	if (!t)
	{
		rend_vk_set_diagnostic(REND_DIAG_CAPACITY, 0,
				       "timeline capacity exhausted");
		return 0;
	}
	memset(&ti, 0, sizeof(ti));
	ti.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
	ti.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
	ti.initialValue = initial;
	memset(&ci, 0, sizeof(ci));
	ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	ci.pNext = &ti;
	r = vkCreateSemaphore(q->backend->device, &ci, NULL, &t->semaphore);
	if (r != VK_SUCCESS)
	{
		rend_vk_set_diagnostic(REND_DIAG_NATIVE, r,
				       "vkCreateSemaphore timeline failed");
		return 0;
	}
	t->backend = q->backend;
	t->producer = q;
	t->last_signal = initial;
	t->live = 1;
	*out = t;
	return 1;
}
int
rend_timeline_query(RendTimeline t, uint64_t *out)
{
	uint32_t i;
	if (!out || !t || !t->live)
		return 0;
	if (vkGetSemaphoreCounterValue(t->backend->device, t->semaphore, out) !=
	    VK_SUCCESS)
		return 0;
	for (i = 0; i < t->backend->queue_count; i++)
		rend_queue_reap(&t->backend->queues[i]);
	return 1;
}
int
rend_timeline_wait(RendTimeline t, uint64_t value, uint64_t timeout)
{
	VkSemaphoreWaitInfo wi;
	VkResult r;
	if (!t || !t->live)
		return 0;
	memset(&wi, 0, sizeof(wi));
	wi.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
	wi.semaphoreCount = 1;
	wi.pSemaphores = &t->semaphore;
	wi.pValues = &value;
	r = vkWaitSemaphores(t->backend->device, &wi, timeout);
	if (r == VK_TIMEOUT)
	{
		rend_vk_set_diagnostic(REND_DIAG_TIMEOUT, r, "timeline wait timed out");
		return 0;
	}
	if (r != VK_SUCCESS)
	{
		t->backend->terminal = 1;
		rend_vk_set_diagnostic(REND_DIAG_TERMINAL, r,
				       "timeline wait native/device failure");
		return 0;
	}
	{
		uint32_t i;
		for (i = 0; i < t->backend->queue_count; i++)
			rend_queue_reap(&t->backend->queues[i]);
	}
	return 1;
}
int
rend_timeline_destroy(RendTimeline t)
{
	RendBackend b;
	uint32_t i, j;
	if (!t || !t->live || !(b = t->backend) || b->magic != REND_BACKEND_MAGIC)
		return 0;
	for (i = 0; i < b->queue_count; i++)
	{
		uint32_t k;
		rend_queue_reap(&b->queues[i]);
		for (k = 0; k < b->queues[i].submission_capacity; k++)
		{
			RendQueueSubmission *s = &b->queues[i].submissions[k];
			uint32_t w;
			if (!s->live)
				continue;
			if (s->timeline == t)
				return 0;
			for (w = 0; w < s->wait_count; w++)
				if (b->queues[i].wait_refs[k * b->queues[i].max_waits + w].timeline ==
				    t)
					return 0;
		}
	}
	for (i = 0; i < b->presentation_count; i++)
	{
		uint32_t k;
		RendPresentationRecord *p = &b->presentations[i];
		if (!p->live)
			continue;
		for (k = 0; k < 8; k++)
			if (p->acquire_timeline[k] == t)
			{
				uint64_t done = 0;
				if (vkGetSemaphoreCounterValue(b->device, t->semaphore, &done) !=
					    VK_SUCCESS ||
				    done < p->acquire_values[k])
					return 0;
				p->acquire_timeline[k] = NULL;
			}
	}
	for (i = 0; i < b->pool_count; i++)
		for (j = 0; j < b->pools[i].count; j++)
			if (b->pools[i].commands[j].completion == t &&
			    b->pools[i].commands[j].state)
				return 0;
	vkDestroySemaphore(b->device, t->semaphore, NULL);
	memset(t, 0, sizeof(*t));
	return 1;
}
int
rend_queue_submit(RendQueue q, const RendSubmitDesc *d)
{
	RendBackend b;
	uint32_t i, j, slot_index = UINT32_MAX;
	VkCommandBufferSubmitInfo *cmds;
	VkSemaphoreSubmitInfo *waits, *signals;
	VkSubmitInfo2 submit;
	VkResult r;
	RendTimeline completion;
	RendTimelinePoint point;
	RendPresentationRecord *presentation = NULL;
	uint32_t wait_count, signal_count;
	if (!q || !q->live || !d || (b = q->backend)->magic != REND_BACKEND_MAGIC ||
	    b->terminal || !d->commands || !d->command_count ||
	    d->command_count > q->max_submit_lists || d->wait_count > q->max_waits ||
	    (!d->waits && d->wait_count) || !d->completion.timeline)
		return 0;
	completion = d->completion.timeline;
	point = d->completion;
	if (!completion->live || completion->backend != b ||
	    completion->producer != q || point.value <= completion->last_signal)
		return 0;
	VkCommandBufferSubmitInfo cmd_storage[64];
	VkSemaphoreSubmitInfo wait_storage[64], signal_storage[2];
	if (d->command_count > 64 || d->wait_count > 64)
		return 0;
	cmds = cmd_storage;
	waits = wait_storage;
	signals = signal_storage;
	rend_queue_reap(q);
	for (i = 0; i < q->submission_capacity; i++)
		if (!q->submissions[i].live)
		{
			slot_index = i;
			break;
		}
	if (slot_index == UINT32_MAX)
		return 0;
	for (i = 0; i < d->command_count; i++)
	{
		RendCommandRecord *c = d->commands[i];
		if (!c || c->backend != b || c->state != 2 || c->pool->family != q->family)
			return 0;
		if (c->presentation)
		{
			if ((presentation && presentation != c->presentation) ||
			    !c->presentation->acquired || c->presentation->submitted)
				return 0;
			presentation = c->presentation;
		}
		memset(&cmds[i], 0, sizeof(cmds[i]));
		cmds[i].sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
		cmds[i].commandBuffer = c->command;
		for (j = 0; j < i; j++)
			if (d->commands[j] == c)
				return 0;
	}
	wait_count = d->wait_count + (presentation ? 1u : 0u);
	signal_count = 1 + (presentation ? 1u : 0u);
	if (wait_count > 64 || d->wait_count > q->max_waits)
		return 0;
	for (i = 0; i < d->wait_count; i++)
	{
		RendTimeline t = d->waits[i].point.timeline;
		if (!t || !t->live || t->backend != b || t == completion ||
		    !d->waits[i].consumer_stages ||
		    (d->waits[i].consumer_stages &
		     ~(REND_STAGE_TRANSFER | REND_STAGE_COMPUTE | REND_STAGE_INDEX_INPUT |
		       REND_STAGE_VERTEX | REND_STAGE_FRAGMENT | REND_STAGE_COLOR_OUTPUT |
		       REND_STAGE_INDIRECT | REND_STAGE_GRAPHICS)))
			return 0;
		for (j = 0; j < i; j++)
			if (d->waits[j].point.timeline == t)
				return 0;
		memset(&waits[i], 0, sizeof(waits[i]));
		waits[i].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
		waits[i].semaphore = t->semaphore;
		waits[i].value = d->waits[i].point.value;
		waits[i].stageMask = rend_vk_stage(d->waits[i].consumer_stages);
	}
	if (presentation)
	{
		memset(&waits[d->wait_count], 0, sizeof(waits[0]));
		waits[d->wait_count].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
		waits[d->wait_count].semaphore =
			presentation->acquire_semaphores[presentation->acquire_slot];
		waits[d->wait_count].stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	}
	memset(signal_storage, 0, sizeof(signal_storage));
	signals[0].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	signals[0].semaphore = completion->semaphore;
	signals[0].value = point.value;
	signals[0].stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	if (presentation)
	{
		signals[1].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
		signals[1].semaphore =
			presentation->render_semaphores[presentation->image_index];
		signals[1].stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	}
	memset(&submit, 0, sizeof(submit));
	submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	submit.waitSemaphoreInfoCount = wait_count;
	submit.pWaitSemaphoreInfos = waits;
	submit.commandBufferInfoCount = d->command_count;
	submit.pCommandBufferInfos = cmds;
	submit.signalSemaphoreInfoCount = signal_count;
	submit.pSignalSemaphoreInfos = signals;
	b->drained = 0;
	r = vkQueueSubmit2(q->queue, 1, &submit, VK_NULL_HANDLE);
	if (r != VK_SUCCESS)
	{
		RendQueueSubmission *slot = &q->submissions[slot_index];
		memset(slot, 0, sizeof(*slot));
		slot->live = 1;
		slot->uncertain = 1;
		slot->timeline = completion;
		slot->value = point.value;
		slot->wait_count = d->wait_count;
		for (i = 0; i < d->wait_count; i++)
		{
			q->wait_refs[slot_index * q->max_waits + i].timeline =
				d->waits[i].point.timeline;
			q->wait_refs[slot_index * q->max_waits + i].value =
				d->waits[i].point.value;
		}
		q->active_submissions++;
		for (i = 0; i < d->command_count; i++)
		{
			RendCommandRecord *c = d->commands[i];
			c->state = 3;
			c->completion = completion;
			c->completion_value = point.value;
		}
		b->terminal = 1;
		rend_vk_set_diagnostic(REND_DIAG_TERMINAL, r,
				       "native queue submission failure; context terminal "
				       "and backing retained");
		return 0;
	}
	completion->last_signal = point.value;
	if (presentation)
	{
		presentation->submitted = 1;
		presentation->acquire_timeline[presentation->acquire_slot] = completion;
		presentation->acquire_values[presentation->acquire_slot] = point.value;
	}
	{
		RendQueueSubmission *slot = &q->submissions[slot_index];
		memset(slot, 0, sizeof(*slot));
		slot->live = 1;
		slot->timeline = completion;
		slot->value = point.value;
		slot->wait_count = d->wait_count;
		for (i = 0; i < d->wait_count; i++)
		{
			q->wait_refs[slot_index * q->max_waits + i].timeline =
				d->waits[i].point.timeline;
			q->wait_refs[slot_index * q->max_waits + i].value =
				d->waits[i].point.value;
		}
		q->active_submissions++;
	}
	for (i = 0; i < d->command_count; i++)
	{
		RendCommandRecord *c = d->commands[i];
		c->state = 3;
		c->completion = completion;
		c->completion_value = point.value;
	}
	return 1;
}

int
rend_backend_wait_idle(RendBackend b)
{
	uint32_t i, j;
	VkResult r;
	if (!b || b->magic != REND_BACKEND_MAGIC)
		return 0;
	r = vkDeviceWaitIdle(b->device);
	if (r != VK_SUCCESS)
	{
		rend_vk_set_diagnostic(
			REND_DIAG_NATIVE, r,
			"explicit vkDeviceWaitIdle failed; retain native backing");
		return 0;
	}
	b->drained = 1;
	for (i = 0; i < b->presentation_count; i++)
	{
		uint32_t k;
		for (k = 0; k < 8; k++)
			b->presentations[i].acquire_timeline[k] = NULL;
	}
	for (i = 0; i < b->pool_count; i++)
		for (j = 0; j < b->pools[i].count; j++)
			if (b->pools[i].commands[j].state == 3)
				b->pools[i].commands[j].state = 4;
	for (i = 0; i < b->queue_count; i++)
	{
		b->queues[i].active_submissions = 0;
		for (j = 0; j < b->queues[i].submission_capacity; j++)
			b->queues[i].submissions[j].live = 0;
	}
	return 1;
}

int
rend_backend_destroy(RendBackend b)
{
	uint32_t i, j;
	if (!b || b->magic != REND_BACKEND_MAGIC)
		return 0;
	for (i = 0; i < b->queue_count; i++)
		rend_queue_reap(&b->queues[i]);
	for (i = 0; i < b->pool_count; i++)
		for (j = 0; j < b->pools[i].count; j++)
			if (b->pools[i].commands[j].state)
			{
				rend_vk_set_diagnostic(
					REND_DIAG_INVALID_ARGUMENT, 0,
					"command pools must be reset before backend destruction");
				return 0;
			}
	for (i = 0; i < b->heap_count; i++)
		if (b->heaps[i].live)
			return 0;
	for (i = 0; i < b->texture_count; i++)
		if (b->textures[i].live)
			return 0;
	for (i = 0; i < b->view_count; i++)
		if (b->views[i].live)
			return 0;
	for (i = 0; i < b->pipeline_count; i++)
		if (b->pipelines[i].live)
			return 0;
	for (i = 0; i < b->timeline_count; i++)
		if (b->timelines[i].live)
			return 0;
	for (i = 0; i < b->presentation_count; i++)
		if (b->presentations[i].live)
			return 0;
	for (i = 0; i < b->queue_count; i++)
		if (b->queues[i].active_submissions)
		{
			rend_vk_set_diagnostic(
				REND_DIAG_INVALID_ARGUMENT, 0,
				"backend has pending work; call explicit wait_idle first");
			return 0;
		}
	for (i = 0; i < b->view_count; i++)
		if (b->views[i].live)
			vkDestroyImageView(b->device, b->views[i].view, NULL);
	for (i = 0; i < b->texture_count; i++)
		if (b->textures[i].live)
			vkDestroyImage(b->device, b->textures[i].image, NULL);
	for (i = 0; i < b->pipeline_count; i++)
		if (b->pipelines[i].live)
			vkDestroyPipeline(b->device, b->pipelines[i].pipeline, NULL);
	for (i = 0; i < b->timeline_count; i++)
		if (b->timelines[i].live)
			vkDestroySemaphore(b->device, b->timelines[i].semaphore, NULL);
	for (i = 0; i < b->heap_count; i++)
		if (b->heaps[i].live)
		{
			RendHeapRecord *h = &b->heaps[i];
			if (h->mapped)
				vkUnmapMemory(b->device, h->memory);
			if (h->buffer)
				vkDestroyBuffer(b->device, h->buffer, NULL);
			vkFreeMemory(b->device, h->memory, NULL);
		}
	for (i = 0; i < b->pool_count; i++)
		if (b->pools[i].pool)
			vkDestroyCommandPool(b->device, b->pools[i].pool, NULL);
	vkDestroyDevice(b->device, NULL);
	if (b->device_info->owner && b->device_info->owner->context_count)
		b->device_info->owner->context_count--;
	b->magic = 0;
	b->device = VK_NULL_HANDLE;
	return 1;
}
