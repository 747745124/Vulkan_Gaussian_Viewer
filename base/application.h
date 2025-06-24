#pragma once
#define GLFW_INCLUDE_VULKAN
#include <chrono>
#include <string>
#include <iostream>
#include <vector>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <set>
#include "input.h"
#include "utility.hpp"
#include "swapChainUtils.hpp"
#include "shaderUtils.hpp"
#include "vertex.h"

#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_EXPOSE_NATIVE_WIN32
#elif __APPLE__
#define VK_USE_PLATFORM_MACOS_MVK
#define GLFW_EXPOSE_NATIVE_COCOA
#include <vulkan/vulkan_macos.h>
#include <vulkan/vulkan_beta.h>
#include "ObjC-interface.h"
#elif __linux__
#define VK_USE_PLATFORM_XLIB_KHR
#define GLFW_EXPOSE_NATIVE_X11
#endif

#include <GLFW/glfw3native.h>

#define GLM_FORCE_RADIANS
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>

class Application
{
private:
	VkInstance _instance;
	VkPhysicalDevice _physicalDevice = VK_NULL_HANDLE;
	VkDevice _device;
	VkQueue _graphicsQueue;
	VkQueue _presentQueue;
	VkSurfaceKHR _surface;

	VkSwapchainKHR _swapChain;
	std::vector<VkImage> _swapChainImages;
	VkFormat _swapChainImageFormat;
	VkExtent2D _swapChainExtent;
	std::vector<VkImageView> _swapChainImageViews;

	VkPipelineLayout _pipelineLayout;
	VkRenderPass _renderPass;
	VkPipeline _graphicsPipeline;
	VkCommandPool _commandPool;
	std::vector<VkFramebuffer> _swapChainFramebuffers;

	const uint32_t MAX_FRAMES_IN_FLIGHT = 3;
	uint32_t _currentFrame = 0;
	std::vector<VkCommandBuffer> _commandBuffers;
	std::vector<VkSemaphore> _imageAvailableSemaphores;
	std::vector<VkSemaphore> _renderFinishedSemaphores;
	std::vector<VkFence> _inFlightFences;

	// GPU memory handles
	std::vector<VkBuffer> _uniformBuffers;
	// actual GPU memory allocated for the uniform buffers
	std::vector<VkDeviceMemory> _uniformBuffersMemory;
	// CPU pointer to the GPU memory
	std::vector<void *> _uniformBuffersMapped;

	VkBuffer _vertexBuffer;
	VkDeviceMemory _vertexBufferMemory;

	VkBuffer _indexBuffer;
	VkDeviceMemory _indexBufferMemory;

	VkDescriptorSetLayout _descriptorSetLayout;
	std::vector<VkDescriptorSet> _descriptorSets;
	VkDescriptorPool _descriptorPool;

	void createInstance();
	void createSurface();
	void selectPhysicalDevice(uint32_t deviceIndex = 0);
	void createLogicalDevice();
	void createRenderPass();
	void createGraphicsPipeline();
	void createFramebuffers();
	void createCommandPool();
	void createDescriptorPool();

	void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
	void createSyncObjects();
	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer &buffer, VkDeviceMemory &bufferMemory);
	void cleanupSwapChain();
	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
	void recreateSwapChain();
	void createIndexBuffer();
	void createVertexBuffer();
	// note that uniform buffers usually vary for each frame
	void createUniformBuffers();
	void createCommandBuffer();
	// descriptor set layout
	void createDescriptorSetLayout();
	void createDescriptorSets();
	void updateUniformBuffer(uint32_t currentFrame);

	bool checkValidationLayerSupport(const std::vector<const char *> &validationLayers);
	void printInstanceExtensionSupport();
	bool checkDeviceExtensionSupport(const VkPhysicalDevice &device, const std::vector<const char *> &requiredExtensions);
	bool isDeviceSuitable(const VkPhysicalDevice &device, const VkSurfaceKHR &surface);

public:
	Application();

	virtual ~Application();

	void run();

protected:
	/* window info */
	GLFWwindow *_window = nullptr;
	std::string _windowTitle;
	int _windowWidth = 2000;
	int _windowHeight = 1200;
	bool _windowReized = false;

	/* timer for fps */
	std::chrono::time_point<std::chrono::high_resolution_clock> _lastTimeStamp;
	float _deltaTime = 0.0f;

	/* input handler */
	KeyboardInput _keyboardInput;
	MouseInput _mouseInput;

	/* clear color */
	glm::vec4 _clearColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

	void updateTime();

	/* derived class can override this function to handle input */
	virtual void handleInput() {};

	/* derived class can override this function to render a frame */
	virtual void renderFrame();

	void showFpsInWindowTitle();

	static void framebufferResizeCallback(GLFWwindow *window, int width, int height);

	static void cursorMovedCallback(GLFWwindow *window, double xPos, double yPos);

	static void mouseClickedCallback(GLFWwindow *window, int button, int action, int mods);

	static void scrollCallback(GLFWwindow *window, double xOffset, double yOffset);

	static void keyboardCallback(GLFWwindow *window, int key, int scancode, int action, int mods);
};