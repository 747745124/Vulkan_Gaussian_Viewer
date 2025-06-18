#pragma once
#define GLFW_INCLUDE_VULKAN
#include <chrono>
#include <string>
#include <iostream>
#include <vector>
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include "input.h"
#include "utility.hpp"

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

class Application
{
private:
	VkInstance _instance;
	VkPhysicalDevice _physicalDevice = VK_NULL_HANDLE;
	VkDevice _device;
	VkQueue _graphicsQueue;
	VkSurfaceKHR _surface;
	void createInstance();
	void createSurface();
	void selectPhysicalDevice(uint32_t deviceIndex = 0);
	void createLogicalDevice();

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
	virtual void handleInput() = 0;

	/* derived class can override this function to render a frame */
	virtual void renderFrame() = 0;

	void showFpsInWindowTitle();

	static void framebufferResizeCallback(GLFWwindow *window, int width, int height);

	static void cursorMovedCallback(GLFWwindow *window, double xPos, double yPos);

	static void mouseClickedCallback(GLFWwindow *window, int button, int action, int mods);

	static void scrollCallback(GLFWwindow *window, double xOffset, double yOffset);

	static void keyboardCallback(GLFWwindow *window, int key, int scancode, int action, int mods);
};