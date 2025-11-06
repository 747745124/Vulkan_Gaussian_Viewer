#pragma once

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <vector>
#include <array>

// MVP matrix
struct MVPMatrix
{
	alignas(16) glm::mat4 model;
	alignas(16) glm::mat4 view;
	alignas(16) glm::mat4 proj;
};

// Splat (billboard) data for PLY rendering
struct SplatInstance { 
	glm::vec3 center; 
	glm::vec3 color; 
	float radius; 
	glm::vec3 scale; 
	glm::vec4 rot; 
	float opacity; 
};