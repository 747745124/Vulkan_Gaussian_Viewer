#include "application.h"
#include "ply_parser.h"
#define STB_IMAGE_IMPLEMENTATION
#include "external/stb_image.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "external/tiny_obj_loader.h"

void Application::createShaderStorageBuffers()
{
	_shaderStorageBuffers.resize(MAX_FRAMES_IN_FLIGHT);
	_shaderStorageBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
	_shaderStorageBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

	VkDeviceSize sz = sizeof(uint32_t) * _indices.size();
	if (sz == 0)
	{
		// still create tiny host-visible buffers to keep descriptor setup simple
		sz = sizeof(uint32_t);
	}

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		createBuffer(sz, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _shaderStorageBuffers[i], _shaderStorageBuffersMemory[i]);
		vkMapMemory(_device, _shaderStorageBuffersMemory[i], 0, sz, 0, &_shaderStorageBuffersMapped[i]);
	}
};

void Application::loadModel()
{
	auto ends_with = [](const std::string &s, const std::string &suffix){
		return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
	};

	_vertices.clear();
	_indices.clear();
	_splatInstances.clear();

	if (ends_with(_modelPath, ".ply"))
	{
		// Load PLY points using our parser
		std::vector<Gaussian> gs;
		ParseOptions opts;
		opts.scaleSpace = ScaleSpace::Linear;
		opts.rotationOrder = RotationOrder::XYZW;
		if (!parse_ply(_modelPath, gs, opts))
		{
			throw std::runtime_error("Failed to load PLY: " + _modelPath);
		}
		_vertices.reserve(gs.size());
		_splatInstances.reserve(gs.size());
		for (const auto &g : gs)
		{
			Vertex v{};
			v.position = g.position;
			v.normal = glm::vec3(0.0f, 0.0f, 1.0f);
			v.texCoord = glm::vec2(0.0f);
			v.color = glm::clamp(g.f_dc_0, glm::vec3(0.0f), glm::vec3(1.0f));
			_vertices.push_back(v);

			SplatInstance inst{};
			inst.center = g.position;
			inst.color = glm::clamp(g.f_dc_0, glm::vec3(0.0f), glm::vec3(1.0f));
			float r = g.scale.x;
			if (!std::isfinite(r) || r <= 0.0f) r = 0.01f;
			inst.radius = r;
			_splatInstances.push_back(inst);
		}
		return; // indices not used for point/splat cloud
	}

	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string warn, err;

	if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, _modelPath.c_str()))
	{
		throw std::runtime_error(warn + err);
	}

	std::unordered_map<Vertex, uint32_t> uniqueVertices{};

	for (const auto &shape : shapes)
	{
		for (const auto &index : shape.mesh.indices)
		{
			Vertex vertex = {};
			vertex.position = {
				attrib.vertices[3 * index.vertex_index + 0],
				attrib.vertices[3 * index.vertex_index + 1],
				attrib.vertices[3 * index.vertex_index + 2]};

			if (index.normal_index >= 0)
			{
				vertex.normal = {
					attrib.normals[3 * index.normal_index + 0],
					attrib.normals[3 * index.normal_index + 1],
					attrib.normals[3 * index.normal_index + 2]};
			}

			if (index.texcoord_index >= 0)
			{
				vertex.texCoord = {
					attrib.texcoords[2 * index.texcoord_index + 0],
					1.0f - attrib.texcoords[2 * index.texcoord_index + 1]};
			}

			vertex.color = {1.0f, 1.0f, 1.0f};

			if (uniqueVertices.count(vertex) == 0)
			{
				uniqueVertices[vertex] = static_cast<uint32_t>(_vertices.size());
				_vertices.push_back(vertex);
			}

			_indices.push_back(uniqueVertices[vertex]);
		}
	}
}

void Application::createSplatBuffers()
{
	if (_splatInstances.empty()) return;
	// Quad corners (triangle strip order)
	std::array<glm::vec2, 4> corners = { glm::vec2(-1.f, -1.f), glm::vec2(1.f, -1.f), glm::vec2(-1.f, 1.f), glm::vec2(1.f, 1.f) };

	// Vertex buffer for corners
	{
		VkDeviceSize bufferSize = sizeof(corners);
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
		void *data;
		vkMapMemory(_device, stagingBufferMemory, 0, bufferSize, 0, &data);
		memcpy(data, corners.data(), bufferSize);
		vkUnmapMemory(_device, stagingBufferMemory);
		createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _splatVertexBuffer, _splatVertexBufferMemory);
		copyBuffer(stagingBuffer, _splatVertexBuffer, bufferSize);
		vkDestroyBuffer(_device, stagingBuffer, nullptr);
		vkFreeMemory(_device, stagingBufferMemory, nullptr);
	}

	// Instance buffer
	{
		VkDeviceSize bufferSize = sizeof(SplatInstance) * _splatInstances.size();
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
		void *data;
		vkMapMemory(_device, stagingBufferMemory, 0, bufferSize, 0, &data);
		memcpy(data, _splatInstances.data(), bufferSize);
		vkUnmapMemory(_device, stagingBufferMemory);
		createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _splatInstanceBuffer, _splatInstanceBufferMemory);
		copyBuffer(stagingBuffer, _splatInstanceBuffer, bufferSize);
		vkDestroyBuffer(_device, stagingBuffer, nullptr);
		vkFreeMemory(_device, stagingBufferMemory, nullptr);
	}
}

void Application::createDepthResources()
{
	VkFormat depthFormat = Utils::findDepthFormat(_physicalDevice);
	createImage(_swapChainExtent.width, _swapChainExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _depthImage, _depthImageMemory);

	_depthImageView = Utils::createImageView(_device, _depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
	transitionImageLayout(_depthImage, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
}

void Application::createTextureSampler()
{
	VkSamplerCreateInfo samplerInfo = {};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.anisotropyEnable = VK_TRUE;

	VkPhysicalDeviceProperties properties = {};
	vkGetPhysicalDeviceProperties(_physicalDevice, &properties);
	samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 0.0f;

	if (vkCreateSampler(_device, &samplerInfo, nullptr, &_textureSampler) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create texture sampler!");
	}
};

void Application::createTextureImageView()
{
	_textureImageView = Utils::createImageView(_device, _textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
}

void Application::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
{
	VkCommandBuffer commandBuffer = Utils::beginSingleTimeCommands(_device, _commandPool);

	VkBufferImageCopy region = {};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;

	region.imageOffset = {0, 0, 0};
	region.imageExtent = {width, height, 1};

	vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	Utils::endSingleTimeCommands(_device, _commandPool, _graphicsQueue, commandBuffer);
};

void Application::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
{
	VkCommandBuffer commandBuffer = Utils::beginSingleTimeCommands(_device, _commandPool);

	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;

	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

	barrier.image = image;

	if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
	{
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

		if (Utils::hasStencilComponent(format))
		{
			barrier.subresourceRange.aspectMask |=
				VK_IMAGE_ASPECT_STENCIL_BIT;
		}
	}
	else
	{
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	}

	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	barrier.srcAccessMask = 0;
	barrier.dstAccessMask = 0;

	VkPipelineStageFlags sourceStage;
	VkPipelineStageFlags destinationStage;

	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	}
	else
	{
		throw std::invalid_argument("unsupported layout transition!");
	}

	vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
	Utils::endSingleTimeCommands(_device, _commandPool, _graphicsQueue, commandBuffer);
}

void Application::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage &image, VkDeviceMemory &imageMemory)

{
	VkImageCreateInfo imageInfo = {};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = format;
	imageInfo.tiling = tiling;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = usage;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.flags = 0;

	if (vkCreateImage(_device, &imageInfo, nullptr, &image) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create image!");
	}

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(_device, image, &memRequirements);

	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = Utils::findMemoryType(_physicalDevice, memRequirements.memoryTypeBits, properties);

	if (vkAllocateMemory(_device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate image memory!");
	}

	vkBindImageMemory(_device, image, imageMemory, 0);
}

void Application::createTextureImage(const std::string &texturePath)
{
	int texWidth, texHeight, texChannels;
	stbi_uc *pixels = stbi_load(texturePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

	if (!pixels)
	{
		throw std::runtime_error("failed to load texture image!");
	}

	VkDeviceSize imageSize = texWidth * texHeight * 4;

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
	// loading to the staging buffer
	void *data;
	vkMapMemory(_device, stagingBufferMemory, 0, imageSize, 0, &data);
	memcpy(data, pixels, static_cast<size_t>(imageSize));
	vkUnmapMemory(_device, stagingBufferMemory);

	stbi_image_free(pixels);

	// create image
	createImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _textureImage, _textureImageMemory);
	transitionImageLayout(_textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	copyBufferToImage(stagingBuffer, _textureImage, texWidth, texHeight);
	transitionImageLayout(_textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	vkDestroyBuffer(_device, stagingBuffer, nullptr);
	vkFreeMemory(_device, stagingBufferMemory, nullptr);
};

void Application::createDescriptorSets()
{
	std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, _descriptorSetLayout);
	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = _descriptorPool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
	allocInfo.pSetLayouts = layouts.data();

	_descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
	if (vkAllocateDescriptorSets(_device, &allocInfo, _descriptorSets.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate descriptor sets!");
	}

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		VkDescriptorBufferInfo bufferInfo = {};
		bufferInfo.buffer = _uniformBuffers[i];
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(UniformBufferObject);

		VkDescriptorImageInfo imageInfo = {};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = _textureImageView;
		imageInfo.sampler = _textureSampler;

		std::array<VkWriteDescriptorSet, 2> descriptorWrites = {};
		descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[0].dstSet = _descriptorSets[i];
		descriptorWrites[0].dstBinding = 0;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].pBufferInfo = &bufferInfo;
		descriptorWrites[0].pImageInfo = nullptr;
		descriptorWrites[0].pTexelBufferView = nullptr;

		descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[1].dstSet = _descriptorSets[i];
		descriptorWrites[1].dstBinding = 1;
		descriptorWrites[1].dstArrayElement = 0;
		descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[1].descriptorCount = 1;
		descriptorWrites[1].pImageInfo = &imageInfo;

		vkUpdateDescriptorSets(_device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
	}

	return;
};

void Application::createDescriptorPool()
{
	std::array<VkDescriptorPoolSize, 2> poolSizes = {};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();

	poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

	if (vkCreateDescriptorPool(_device, &poolInfo, nullptr, &_descriptorPool) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create descriptor pool!");
	}

	return;
};

void Application::updateUniformBuffer(uint32_t currentFrame)
{
	// only called once
	static auto startTime = std::chrono::high_resolution_clock::now();
	// called every frame
	auto currentTime = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

	UniformBufferObject ubo = {};
	ubo.model = glm::mat4(1.0f);
	ubo.view = glm::lookAt(glm::vec3(0.f, 0.f, 3.f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	ubo.proj = glm::perspective(glm::radians(60.0f), _swapChainExtent.width / (float)_swapChainExtent.height, 0.1f, 100.0f);
	ubo.proj[1][1] *= -1;

	memcpy(_uniformBuffersMapped[currentFrame], &ubo, sizeof(ubo));
};

void Application::createUniformBuffers()
{
	VkDeviceSize bufferSize = sizeof(UniformBufferObject);
	_uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
	_uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
	_uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _uniformBuffers[i], _uniformBuffersMemory[i]);
		vkMapMemory(_device, _uniformBuffersMemory[i], 0, bufferSize, 0, &_uniformBuffersMapped[i]);
	}
}

void Application::createDescriptorSetLayout()
{
	VkDescriptorSetLayoutBinding uboLayoutBinding = {};
	uboLayoutBinding.binding = 0;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.descriptorCount = 1;
	// only one UBO (i.e. MVP matrix)
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	uboLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
	samplerLayoutBinding.binding = 1;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	samplerLayoutBinding.pImmutableSamplers = nullptr;

	std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding, samplerLayoutBinding};

	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(_device, &layoutInfo, nullptr, &_descriptorSetLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create descriptor set layout!");
	}

	return;
}

void Application::createIndexBuffer()
{
	if (_indices.empty())
	{
		_indexBuffer = VK_NULL_HANDLE;
		_indexBufferMemory = VK_NULL_HANDLE;
		return;
	}
	VkDeviceSize bufferSize = sizeof(_indices[0]) * _indices.size();
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

	void *data;
	vkMapMemory(_device, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, _indices.data(), bufferSize);
	vkUnmapMemory(_device, stagingBufferMemory);

	createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _indexBuffer, _indexBufferMemory);
	copyBuffer(stagingBuffer, _indexBuffer, bufferSize);

	vkDestroyBuffer(_device, stagingBuffer, nullptr);
	vkFreeMemory(_device, stagingBufferMemory, nullptr);
}
// this is a temporary one-time command buffer
void Application::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
	VkCommandBuffer commandBuffer = Utils::beginSingleTimeCommands(_device, _commandPool);

	VkBufferCopy copyRegion = {};
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	copyRegion.size = size;
	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

	Utils::endSingleTimeCommands(_device, _commandPool, _graphicsQueue, commandBuffer);
}

void Application::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer &buffer, VkDeviceMemory &bufferMemory)
{
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateBuffer(_device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create buffer!");
	}

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(_device, buffer, &memRequirements);

	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = Utils::findMemoryType(_physicalDevice, memRequirements.memoryTypeBits, properties);

	if (vkAllocateMemory(_device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate buffer memory!");
	}

	vkBindBufferMemory(_device, buffer, bufferMemory, 0);
}

// create a triangle vertex buffer
void Application::createVertexBuffer()
{
	VkDeviceSize bufferSize = sizeof(_vertices[0]) * _vertices.size();
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	// using staging buffer to copy the triangle vertices to the buffer
	createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

	// copy the triangle vertices to the buffer
	// data is the temporary pointer to the GPU memory
	// usually the GPU memory is not accessible by the CPU
	// so we need to map the memory to the CPU
	void *data;
	vkMapMemory(_device, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, _vertices.data(), bufferSize);
	vkUnmapMemory(_device, stagingBufferMemory);

	createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _vertexBuffer, _vertexBufferMemory);
	copyBuffer(stagingBuffer, _vertexBuffer, bufferSize);

	vkDestroyBuffer(_device, stagingBuffer, nullptr);
	vkFreeMemory(_device, stagingBufferMemory, nullptr);
}

void Application::cleanupSwapChain()
{
	vkDestroyImageView(_device, _depthImageView, nullptr);
	vkDestroyImage(_device, _depthImage, nullptr);
	vkFreeMemory(_device, _depthImageMemory, nullptr);

	for (auto framebuffer : _swapChainFramebuffers)
	{
		vkDestroyFramebuffer(_device, framebuffer, nullptr);
	}
	for (auto imageView : _swapChainImageViews)
	{
		vkDestroyImageView(_device, imageView, nullptr);
	}

	vkDestroySwapchainKHR(_device, _swapChain, nullptr);
}

void Application::recreateSwapChain()
{
	int width = 0, height = 0;
	glfwGetFramebufferSize(_window, &width, &height);
	while (width == 0 || height == 0)
	{
		glfwGetFramebufferSize(_window, &width, &height);
		glfwWaitEvents();
	}

	vkDeviceWaitIdle(_device);
	cleanupSwapChain();
	Utils::createSwapChain(_physicalDevice, _device, _surface, _window, _swapChain, _swapChainImages, _swapChainImageFormat, _swapChainExtent);
	Utils::createImageViews(_device, _swapChainImages, _swapChainImageFormat, _swapChainImageViews);
	createDepthResources();
	createFramebuffers();
}

void Application::createSyncObjects()
{
	_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	// used to handle for the first frame
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		if (vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &_imageAvailableSemaphores[i]) != VK_SUCCESS ||
			vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &_renderFinishedSemaphores[i]) != VK_SUCCESS ||
			vkCreateFence(_device, &fenceInfo, nullptr, &_inFlightFences[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create sync objects!");
		}
	}
}

void Application::renderFrame()
{
	// wait for previous frame to finish
	vkWaitForFences(_device, 1, &_inFlightFences[_currentFrame], VK_TRUE, UINT64_MAX);
	vkResetFences(_device, 1, &_inFlightFences[_currentFrame]);

	// acquire an image from the swap chain
	uint32_t imageIndex;
	VkResult result = vkAcquireNextImageKHR(_device, _swapChain, UINT64_MAX, _imageAvailableSemaphores[_currentFrame], VK_NULL_HANDLE, &imageIndex);

	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		recreateSwapChain();
		return;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
	{
		throw std::runtime_error("failed to acquire swap chain image!");
	}

	// update the uniform buffer
	updateUniformBuffer(_currentFrame);
	// record the command buffer
	vkResetCommandBuffer(_commandBuffers[_currentFrame], 0);
	recordCommandBuffer(_commandBuffers[_currentFrame], imageIndex);

	// submit the command buffer
	VkSubmitInfo submitInfo = {};
	VkSemaphore waitSemaphores[] = {_imageAvailableSemaphores[_currentFrame]};
	VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &_commandBuffers[_currentFrame];

	VkSemaphore signalSemaphores[] = {_renderFinishedSemaphores[_currentFrame]};
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	if (vkQueueSubmit(_graphicsQueue, 1, &submitInfo, _inFlightFences[_currentFrame]) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to submit draw command buffer!");
	}

	// present the image
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;

	VkSwapchainKHR swapChains[] = {_swapChain};
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &imageIndex;
	presentInfo.pResults = nullptr;

	result = vkQueuePresentKHR(_presentQueue, &presentInfo);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || _windowReized)
	{
		_windowReized = false;
		recreateSwapChain();
	}
	else if (result != VK_SUCCESS)
	{
		throw std::runtime_error("failed to present swap chain image!");
	}

	// Advance to the next frame
	_currentFrame = (_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Application::createCommandPool()
{
	QueueFamilyIndices indices = Utils::findQueueFamilyIndex(_physicalDevice, _surface);

	VkCommandPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = indices.graphicsFamily.value();
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	if (vkCreateCommandPool(_device, &poolInfo, nullptr, &_commandPool) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create command pool!");
	}

	return;
}

void Application::createCommandBuffer()
{
	_commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = _commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = static_cast<uint32_t>(_commandBuffers.size());

	if (vkAllocateCommandBuffers(_device, &allocInfo, _commandBuffers.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate command buffer!");
	}

	return;
}

void Application::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = 0;
	beginInfo.pInheritanceInfo = nullptr;
	if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to begin recording command buffer!");
	}

	VkRenderPassBeginInfo renderPassBeginInfo = {};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass = _renderPass;
	renderPassBeginInfo.framebuffer = _swapChainFramebuffers[imageIndex];
	renderPassBeginInfo.renderArea.offset = {0, 0};
	renderPassBeginInfo.renderArea.extent = _swapChainExtent;
	VkClearValue clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
	VkClearValue clearDepth = {1.0f, 0};
	VkClearValue clearValues[2] = {clearColor, clearDepth};
	renderPassBeginInfo.clearValueCount = 2;
	renderPassBeginInfo.pClearValues = clearValues;
	vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _graphicsPipeline);

	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(_swapChainExtent.width);
	viewport.height = static_cast<float>(_swapChainExtent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

	VkRect2D scissor = {};
	scissor.offset = {0, 0};
	scissor.extent = _swapChainExtent;
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipelineLayout, 0, 1, &_descriptorSets[_currentFrame], 0, nullptr);

	auto ends_with = [](const std::string &s, const std::string &suffix){
		return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
	};
	bool useSplats = ends_with(_modelPath, ".ply") && !_splatInstances.empty();

	if (useSplats)
	{
		VkBuffer bufs[] = { _splatVertexBuffer, _splatInstanceBuffer };
		VkDeviceSize offs[] = { 0, 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 2, bufs, offs);
		struct { float w, h; } pc = { (float)_swapChainExtent.width, (float)_swapChainExtent.height };
		vkCmdPushConstants(commandBuffer, _pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc), &pc);
		vkCmdDraw(commandBuffer, 4, static_cast<uint32_t>(_splatInstances.size()), 0, 0);
	}
	else
	{
		VkBuffer vertexBuffers[] = {_vertexBuffer};
		VkDeviceSize offsets[] = {0};
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(commandBuffer, _indexBuffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(_indices.size()), 1, 0, 0, 0);
	}

	vkCmdEndRenderPass(commandBuffer);
	if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to record command buffer!");
	}
}

void Application::createFramebuffers()
{
	_swapChainFramebuffers.resize(_swapChainImageViews.size());

	for (size_t i = 0; i < _swapChainImageViews.size(); i++)
	{
		VkImageView attachments[] = {_swapChainImageViews[i], _depthImageView};

		VkFramebufferCreateInfo framebufferInfo = {};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = _renderPass;
		framebufferInfo.attachmentCount = 2;
		framebufferInfo.pAttachments = attachments;
		framebufferInfo.width = _swapChainExtent.width;
		framebufferInfo.height = _swapChainExtent.height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(_device, &framebufferInfo, nullptr, &_swapChainFramebuffers[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create framebuffer!");
		}
	}
};

void Application::createRenderPass()
{
	VkAttachmentDescription depthAttachment = {};
	depthAttachment.format = Utils::findDepthFormat(_physicalDevice);
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentRef = {};
	depthAttachmentRef.attachment = 1;
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription colorAttachment = {};
	colorAttachment.format = _swapChainImageFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	// clear the color buffer
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	// we don't care about stencil buffer
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	// pass the color buffer to the presentation engine
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;
	subpass.pDepthStencilAttachment = &depthAttachmentRef;

	std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	if (vkCreateRenderPass(_device, &renderPassInfo, nullptr, &_renderPass) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create render pass!");
	}

	return;
}

void Application::createGraphicsPipeline()
{
	auto ends_with = [](const std::string &s, const std::string &suffix){
		return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
	};

	bool useSplats = ends_with(_modelPath, ".ply");

	auto vertShaderCode = shaderUtils::readFile(useSplats ? "/Users/naoyuki/vk_tutorial/shader/splat.vert.spv" : "/Users/naoyuki/vk_tutorial/shader/triangle.vert.spv");
	auto fragShaderCode = shaderUtils::readFile(useSplats ? "/Users/naoyuki/vk_tutorial/shader/splat.frag.spv" : "/Users/naoyuki/vk_tutorial/shader/triangle.frag.spv");

	VkShaderModule vertShaderModule = shaderUtils::createShaderModule(_device, vertShaderCode);
	VkShaderModule fragShaderModule = shaderUtils::createShaderModule(_device, fragShaderCode);

	VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = vertShaderModule;
	vertShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = fragShaderModule;
	fragShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

	// Dynamic state
	std::vector<VkDynamicState> dynamicStates = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR};

	VkPipelineDynamicStateCreateInfo dynamicStateInfo = {};
	dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicStateInfo.pDynamicStates = dynamicStates.data();

	VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	VkVertexInputBindingDescription bindingDescs[2] = {};
	VkVertexInputAttributeDescription attrDescs[7] = {};
	if (useSplats)
	{
		// binding 0: quad corners vec2 per-vertex
		bindingDescs[0].binding = 0;
		bindingDescs[0].stride = sizeof(glm::vec2);
		bindingDescs[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		// binding 1: instance data
		bindingDescs[1].binding = 1;
		bindingDescs[1].stride = sizeof(SplatInstance);
		bindingDescs[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
		vertexInputInfo.vertexBindingDescriptionCount = 2;
		vertexInputInfo.pVertexBindingDescriptions = bindingDescs;

		// location 0: inCorner
		attrDescs[0].binding = 0; attrDescs[0].location = 0; attrDescs[0].format = VK_FORMAT_R32G32_SFLOAT; attrDescs[0].offset = 0;
		// location 1: inCenter
		attrDescs[1].binding = 1; attrDescs[1].location = 1; attrDescs[1].format = VK_FORMAT_R32G32B32_SFLOAT; attrDescs[1].offset = offsetof(SplatInstance, center);
		// location 2: inColor
		attrDescs[2].binding = 1; attrDescs[2].location = 2; attrDescs[2].format = VK_FORMAT_R32G32B32_SFLOAT; attrDescs[2].offset = offsetof(SplatInstance, color);
		// location 3: inRadius
		attrDescs[3].binding = 1; attrDescs[3].location = 3; attrDescs[3].format = VK_FORMAT_R32_SFLOAT; attrDescs[3].offset = offsetof(SplatInstance, radius);
		// location 4: inScale
		attrDescs[4].binding = 1; attrDescs[4].location = 4; attrDescs[4].format = VK_FORMAT_R32G32B32_SFLOAT; attrDescs[4].offset = offsetof(SplatInstance, scale);
		// location 5: inQuat
		attrDescs[5].binding = 1; attrDescs[5].location = 5; attrDescs[5].format = VK_FORMAT_R32G32B32A32_SFLOAT; attrDescs[5].offset = offsetof(SplatInstance, rot);
		// location 6: inOpacity
		attrDescs[6].binding = 1; attrDescs[6].location = 6; attrDescs[6].format = VK_FORMAT_R32_SFLOAT; attrDescs[6].offset = offsetof(SplatInstance, opacity);
		vertexInputInfo.vertexAttributeDescriptionCount = 7;
		vertexInputInfo.pVertexAttributeDescriptions = attrDescs;
	}
	else
	{
		auto vertexInputBindingDescription = Vertex::getBindingDescription();
		auto vertexInputAttributeDescriptions = Vertex::getAttributeDescriptions();
		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.pVertexBindingDescriptions = &vertexInputBindingDescription;
		vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributeDescriptions.size());
		vertexInputInfo.pVertexAttributeDescriptions = vertexInputAttributeDescriptions.data();
	}

	// input assembly
	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = useSplats ? VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	// viewport
	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(_swapChainExtent.width);
	viewport.height = static_cast<float>(_swapChainExtent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.offset = {0, 0};
	scissor.extent = _swapChainExtent;

	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	// rasterizer
	VkPipelineRasterizationStateCreateInfo rasterizer = {};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;

	// MSAA
	VkPipelineMultisampleStateCreateInfo multisampling = {};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	// depth
	VkPipelineDepthStencilStateCreateInfo depthStencil = {};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	if (useSplats)
	{
		depthStencil.depthTestEnable = VK_FALSE;
		depthStencil.depthWriteEnable = VK_FALSE;
	}
	else
	{
		depthStencil.depthTestEnable = VK_TRUE;
		depthStencil.depthWriteEnable = VK_TRUE;
	}
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.stencilTestEnable = VK_FALSE;

	// blending
	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	if (useSplats)
	{
		colorBlendAttachment.blendEnable = VK_TRUE;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
	}
	else
	{
		colorBlendAttachment.blendEnable = VK_FALSE;
	}

	VkPipelineColorBlendStateCreateInfo colorBlending = {};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;

	// pipeline layout (add push constants for splats)
	VkPushConstantRange pcRange = {};
	pcRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	pcRange.offset = 0;
	pcRange.size = sizeof(float) * 2;

	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &_descriptorSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 1;
	pipelineLayoutInfo.pPushConstantRanges = &pcRange;

	if (vkCreatePipelineLayout(_device, &pipelineLayoutInfo, nullptr, &_pipelineLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create pipeline layout!");
	}

	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.pDynamicState = &dynamicStateInfo;
	pipelineInfo.layout = _pipelineLayout;
	pipelineInfo.renderPass = _renderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = -1;

	if (vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_graphicsPipeline) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create graphics pipeline!");
	}

	vkDestroyShaderModule(_device, vertShaderModule, nullptr);
	vkDestroyShaderModule(_device, fragShaderModule, nullptr);

	return;
};

bool Application::checkDeviceExtensionSupport(const VkPhysicalDevice &device, const std::vector<const char *> &requiredExtensions)
{
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

	std::cout << "Checking device extension support..." << std::endl;

#ifdef _VERBOSE
	std::cout << "Available device extensions:" << std::endl;
	for (const auto &extension : availableExtensions)
	{
		std::cout << "\t" << extension.extensionName << std::endl;
	}
#endif

	// Check if all required extensions are available
	bool allExtensionsSupported = std::all_of(requiredExtensions.begin(), requiredExtensions.end(),
											  [&availableExtensions](const char *requiredExtension)
											  {
												  return std::any_of(availableExtensions.begin(), availableExtensions.end(),
																	 [requiredExtension](const VkExtensionProperties &availableExtension)
																	 {
																		 return strcmp(requiredExtension, availableExtension.extensionName) == 0;
																	 });
											  });

	if (!allExtensionsSupported)
	{
		throw std::runtime_error("Required device extension not available");
	}

	std::cout << "All required device extensions are available" << std::endl;
	return true;
}

void Application::createLogicalDevice()
{
	QueueFamilyIndices indices = Utils::findQueueFamilyIndex(_physicalDevice, _surface);
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};
	float queuePriority = 1.0f;

	for (uint32_t queueFamily : uniqueQueueFamilies)
	{
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(queueCreateInfo);
	}

	// Enable features
	VkPhysicalDeviceFeatures deviceFeatures = {};
	deviceFeatures.samplerAnisotropy = VK_TRUE;

	VkDeviceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.pQueueCreateInfos = queueCreateInfos.data();
	createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	createInfo.pEnabledFeatures = &deviceFeatures;

	std::vector<const char *> deviceExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME};

#ifdef __APPLE__
	// Required extensions for macOS
	deviceExtensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
	checkDeviceExtensionSupport(_physicalDevice, deviceExtensions);
#endif

	createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	createInfo.ppEnabledExtensionNames = deviceExtensions.data();

	VkResult result = vkCreateDevice(_physicalDevice, &createInfo, nullptr, &_device);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create logical device! Error code: " + std::to_string(result));
	}

	// Get the graphics and present queue handles from the device
	vkGetDeviceQueue(_device, indices.graphicsFamily.value(), 0, &_graphicsQueue);
	vkGetDeviceQueue(_device, indices.presentFamily.value(), 0, &_presentQueue);
};

bool Application::isDeviceSuitable(const VkPhysicalDevice &device, const VkSurfaceKHR &surface)
{
	// we can query the device properties and features to check if it is suitable
	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(device, &deviceProperties);

	VkPhysicalDeviceFeatures deviceFeatures;
	vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

	QueueFamilyIndices indices = Utils::findQueueFamilyIndex(device, _surface);

	if (!indices.isComplete() || !Utils::isSwapChainSuitable(device, _surface))
		return false;

	// return deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU || deviceFeatures.geometryShader;
	return true;
};

void Application::selectPhysicalDevice(uint32_t deviceIndex)
{
	std::vector<VkPhysicalDevice> devices;
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(_instance, &deviceCount, nullptr);
	devices.resize(deviceCount);
	vkEnumeratePhysicalDevices(_instance, &deviceCount, devices.data());

	if (deviceCount == 0)
	{
		throw std::runtime_error("failed to find GPUs with Vulkan support!");
	}

	if (deviceIndex >= deviceCount)
	{
		throw std::runtime_error("invalid device index!");
	}

	VkPhysicalDevice device = devices[deviceIndex];
	if (!isDeviceSuitable(device, _surface) || device == VK_NULL_HANDLE)
	{
		throw std::runtime_error("selected device is not suitable or is null!");
	}

	_physicalDevice = device;
	return;
};

void Application::createInstance()
{
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = _windowTitle.c_str();
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "No Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	// Global extensions
	uint32_t glfwExtensionCount = 0;
	const char **glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

#ifdef __APPLE__
	// MoltenVK required extensions
	std::vector<const char *> extensions = {
		VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
		VK_MVK_MACOS_SURFACE_EXTENSION_NAME,
		VK_KHR_SURFACE_EXTENSION_NAME,
		VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME};

	for (uint32_t i = 0; i < glfwExtensionCount; i++)
	{
		extensions.emplace_back(glfwExtensions[i]);
	}

	createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
	createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.data();
#else
	createInfo.enabledExtensionCount = glfwExtensionCount;
	createInfo.ppEnabledExtensionNames = glfwExtensions;
#endif

#ifdef _DEBUG
	std::cout << "Checking validation layer support..." << std::endl;

	const std::vector<const char *> validationLayers = {
		"VK_LAYER_KHRONOS_validation"};

	if (!checkValidationLayerSupport(validationLayers))
	{
		throw std::runtime_error("validation layers requested, but not available!");
	}

	std::cout << "Validation layers supported" << std::endl;

	createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
	createInfo.ppEnabledLayerNames = validationLayers.data();

#else
	// skip the validation layers
	createInfo.enabledLayerCount = 0;
#endif

	if (vkCreateInstance(&createInfo, nullptr, &_instance) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create instance!");
	}
};

void Application::createSurface()
{
#ifdef _WIN32
	VkWin32SurfaceCreateInfoKHR createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	createInfo.hwnd = glfwGetWin32Window(_window);
	createInfo.hinstance = GetModuleHandle(nullptr);
	if (vkCreateWin32SurfaceKHR(_instance, &createInfo, nullptr, &_surface) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create window surface!");
	}
#elif __APPLE__
	VkMacOSSurfaceCreateInfoMVK createInfo = {};

	id windowHandle = glfwGetCocoaWindow(_window);
	id viewHandle = getViewFromNSWindowPointer(windowHandle);
	createInfo.sType = VK_STRUCTURE_TYPE_MACOS_SURFACE_CREATE_INFO_MVK;
	createInfo.pView = viewHandle;
	createInfo.flags = 0;
	createInfo.pNext = nullptr;
	PFN_vkCreateMacOSSurfaceMVK vkCreateMacOSSurfaceMVK;
	vkCreateMacOSSurfaceMVK = (PFN_vkCreateMacOSSurfaceMVK)vkGetInstanceProcAddr(_instance, "vkCreateMacOSSurfaceMVK");

	if (!vkCreateMacOSSurfaceMVK)
	{
		throw std::runtime_error("Unabled to get pointer to function: vkCreateMacOSSurfaceMVK");
	}

	if (vkCreateMacOSSurfaceMVK(_instance, &createInfo, nullptr, &_surface) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create surface!");
	}

#elif __linux__
	VkXlibSurfaceCreateInfoKHR createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
	createInfo.dpy = glfwGetX11Display();
	createInfo.window = glfwGetX11Window(_window);
	if (vkCreateXlibSurfaceKHR(_instance, &createInfo, nullptr, &_surface) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create window surface!");
	}
#endif
}

bool Application::checkValidationLayerSupport(const std::vector<const char *> &validationLayers)
{
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	return std::all_of(validationLayers.begin(), validationLayers.end(),
					   [&availableLayers](const char *layerName)
					   {
						   return std::any_of(availableLayers.begin(), availableLayers.end(),
											  [layerName](const VkLayerProperties &layer)
											  {
												  return strcmp(layerName, layer.layerName) == 0;
											  });
					   });
}

void Application::printInstanceExtensionSupport()
{
	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());

	for (const auto &extension : availableExtensions)
	{
		std::cout << extension.extensionName << std::endl;
	}
}

Application::Application()
{
	// Initialize texture resources to VK_NULL_HANDLE
	_textureImage = VK_NULL_HANDLE;
	_textureImageMemory = VK_NULL_HANDLE;
	_textureImageView = VK_NULL_HANDLE;
	_textureSampler = VK_NULL_HANDLE;
	_depthImage = VK_NULL_HANDLE;
	_depthImageView = VK_NULL_HANDLE;
	_depthImageMemory = VK_NULL_HANDLE;

	// GLFW Stuff Initialization
	if (glfwInit() != GLFW_TRUE)
	{
		throw std::runtime_error("init glfw failure");
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	_window = glfwCreateWindow(
		_windowWidth / 2, _windowHeight / 2, _windowTitle.c_str(), nullptr, nullptr);

	if (_window == nullptr)
	{
		throw std::runtime_error("create glfw window failure");
	}

	glfwMakeContextCurrent(_window);
	glfwSetWindowUserPointer(_window, this);

	// Loading vulkan instance
	createInstance();
	createSurface();

	selectPhysicalDevice();
	createLogicalDevice();
	createCommandPool();

	Utils::createSwapChain(_physicalDevice, _device, _surface, _window, _swapChain, _swapChainImages, _swapChainImageFormat, _swapChainExtent);
	Utils::createImageViews(_device, _swapChainImages, _swapChainImageFormat, _swapChainImageViews);

	createRenderPass();
	createDescriptorSetLayout();
	createGraphicsPipeline();
	createDepthResources();
	createFramebuffers();

	loadModel();
	createVertexBuffer();
	createIndexBuffer();
	createSplatBuffers();
	createUniformBuffers();
	createShaderStorageBuffers();

	// Create texture resources
	createTextureImage(_texturePath);
	createTextureImageView();
	createTextureSampler();

	createDescriptorPool();
	createDescriptorSets();
	createCommandBuffer();
	createSyncObjects();

	// GLFW callbacks registration
	int width, height;
	glfwGetFramebufferSize(_window, &width, &height);
	glfwSetFramebufferSizeCallback(_window, framebufferResizeCallback);
	glfwSetKeyCallback(_window, keyboardCallback);
	glfwSetMouseButtonCallback(_window, mouseClickedCallback);
	glfwSetCursorPosCallback(_window, cursorMovedCallback);
	glfwSetScrollCallback(_window, scrollCallback);
	_lastTimeStamp = std::chrono::high_resolution_clock::now();
}

Application::~Application()
{

	cleanupSwapChain();

	if (_textureSampler != VK_NULL_HANDLE)
	{
		vkDestroySampler(_device, _textureSampler, nullptr);
	}

	if (_textureImageView != VK_NULL_HANDLE)
	{
		vkDestroyImageView(_device, _textureImageView, nullptr);
	}

	if (_textureImage != VK_NULL_HANDLE)
	{
		vkDestroyImage(_device, _textureImage, nullptr);
	}

	if (_textureImageMemory != VK_NULL_HANDLE)
	{
		vkFreeMemory(_device, _textureImageMemory, nullptr);
	}

	if (_vertexBuffer != VK_NULL_HANDLE)
	{
		vkDestroyBuffer(_device, _vertexBuffer, nullptr);
	}

	if (_vertexBufferMemory != VK_NULL_HANDLE)
	{
		vkFreeMemory(_device, _vertexBufferMemory, nullptr);
	}

	if (_indexBuffer != VK_NULL_HANDLE)
	{
		vkDestroyBuffer(_device, _indexBuffer, nullptr);
	}

	if (_indexBufferMemory != VK_NULL_HANDLE)
	{
		vkFreeMemory(_device, _indexBufferMemory, nullptr);
	}

	if (_splatVertexBuffer != VK_NULL_HANDLE)
	{
		vkDestroyBuffer(_device, _splatVertexBuffer, nullptr);
	}
	if (_splatVertexBufferMemory != VK_NULL_HANDLE)
	{
		vkFreeMemory(_device, _splatVertexBufferMemory, nullptr);
	}
	if (_splatInstanceBuffer != VK_NULL_HANDLE)
	{
		vkDestroyBuffer(_device, _splatInstanceBuffer, nullptr);
	}
	if (_splatInstanceBufferMemory != VK_NULL_HANDLE)
	{
		vkFreeMemory(_device, _splatInstanceBufferMemory, nullptr);
	}

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		if (_inFlightFences[i] != VK_NULL_HANDLE)
		{
			vkDestroyFence(_device, _inFlightFences[i], nullptr);
		}

		if (_renderFinishedSemaphores[i] != VK_NULL_HANDLE)
		{
			vkDestroySemaphore(_device, _renderFinishedSemaphores[i], nullptr);
		}

		if (_imageAvailableSemaphores[i] != VK_NULL_HANDLE)
		{
			vkDestroySemaphore(_device, _imageAvailableSemaphores[i], nullptr);
		}

		if (_uniformBuffers[i] != VK_NULL_HANDLE)
		{
			vkDestroyBuffer(_device, _uniformBuffers[i], nullptr);
		}

		if (_uniformBuffersMemory[i] != VK_NULL_HANDLE)
		{
			vkFreeMemory(_device, _uniformBuffersMemory[i], nullptr);
		}

		if (_shaderStorageBuffers[i] != VK_NULL_HANDLE)
		{
			vkDestroyBuffer(_device, _shaderStorageBuffers[i], nullptr);
		}
		if (_shaderStorageBuffersMemory[i] != VK_NULL_HANDLE)
		{
			vkFreeMemory(_device, _shaderStorageBuffersMemory[i], nullptr);
		}
	}

	if (_commandPool != VK_NULL_HANDLE)
	{
		vkDestroyCommandPool(_device, _commandPool, nullptr);
	}

	if (_descriptorPool != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorPool(_device, _descriptorPool, nullptr);
	}

	if (_descriptorSetLayout != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorSetLayout(_device, _descriptorSetLayout, nullptr);
	}

	if (_graphicsPipeline != VK_NULL_HANDLE)
	{
		vkDestroyPipeline(_device, _graphicsPipeline, nullptr);
	}

	if (_pipelineLayout != VK_NULL_HANDLE)
	{
		vkDestroyPipelineLayout(_device, _pipelineLayout, nullptr);
	}

	if (_renderPass != VK_NULL_HANDLE)
	{
		vkDestroyRenderPass(_device, _renderPass, nullptr);
	}

	if (_device != VK_NULL_HANDLE)
	{
		vkDestroyDevice(_device, nullptr);
	}

	if (_window != nullptr)
	{
		glfwDestroyWindow(_window);
		_window = nullptr;
	}

	if (_surface != VK_NULL_HANDLE)
	{
		vkDestroySurfaceKHR(_instance, _surface, nullptr);
	}

	if (_instance != VK_NULL_HANDLE)
	{
		vkDestroyInstance(_instance, nullptr);
	}

	glfwTerminate();
}

void Application::run()
{
	while (!glfwWindowShouldClose(_window))
	{
		updateTime();
		handleInput();
		renderFrame();

		// glfwSwapBuffers(_window);
		glfwPollEvents();
	}

	vkDeviceWaitIdle(_device);
}

void Application::updateTime()
{
	auto now = std::chrono::high_resolution_clock::now();
	_deltaTime = 0.001f * std::chrono::duration<float, std::milli>(now - _lastTimeStamp).count();
	_lastTimeStamp = now;
}

void Application::showFpsInWindowTitle()
{
	double fps = 1.0 / _deltaTime;
	std::string detailTitle = _windowTitle + ": " + std::to_string(fps) + " fps";
	glfwSetWindowTitle(_window, detailTitle.c_str());
}

void Application::framebufferResizeCallback(GLFWwindow *window, int width, int height)
{
	Application *app = reinterpret_cast<Application *>(glfwGetWindowUserPointer(window));
	app->_windowWidth = width;
	app->_windowHeight = height;
	app->_windowReized = true;
}

void Application::cursorMovedCallback(GLFWwindow *window, double xPos, double yPos)
{
	Application *app = reinterpret_cast<Application *>(glfwGetWindowUserPointer(window));
	app->_mouseInput.move.xCurrent = xPos;
	app->_mouseInput.move.yCurrent = yPos;
}

void Application::mouseClickedCallback(GLFWwindow *window, int button, int action, int mods)
{
	Application *app = reinterpret_cast<Application *>(glfwGetWindowUserPointer(window));
	if (action == GLFW_PRESS)
	{
		switch (button)
		{
		case GLFW_MOUSE_BUTTON_LEFT:
			app->_mouseInput.click.left = true;
			break;
		case GLFW_MOUSE_BUTTON_MIDDLE:
			app->_mouseInput.click.middle = true;
			break;
		case GLFW_MOUSE_BUTTON_RIGHT:
			app->_mouseInput.click.right = true;
			break;
		}
	}
	else if (action == GLFW_RELEASE)
	{
		switch (button)
		{
		case GLFW_MOUSE_BUTTON_LEFT:
			app->_mouseInput.click.left = false;
			break;
		case GLFW_MOUSE_BUTTON_MIDDLE:
			app->_mouseInput.click.middle = false;
			break;
		case GLFW_MOUSE_BUTTON_RIGHT:
			app->_mouseInput.click.right = false;
			break;
		}
	}
}

void Application::scrollCallback(GLFWwindow *window, double xOffset, double yOffset)
{
	Application *app = reinterpret_cast<Application *>(glfwGetWindowUserPointer(window));
	app->_mouseInput.scroll.x += xOffset;
	app->_mouseInput.scroll.y += yOffset;
}

void Application::keyboardCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
	if (key != GLFW_KEY_UNKNOWN)
	{
		Application *app = reinterpret_cast<Application *>(glfwGetWindowUserPointer(window));
		app->_keyboardInput.keyStates[key] = action;
	}
}