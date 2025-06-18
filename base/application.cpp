#include "application.h"

void Application::createLogicalDevice()
{
	QueueFamilyIndices indices = Utils::findQueueFamilyIndex(_physicalDevice);
	VkDeviceQueueCreateInfo queueCreateInfo = {};
	float queuePriority = 1.0f;
	queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfo.queueFamilyIndex = indices.graphicsFamily.value();
	queueCreateInfo.queueCount = 1;
	queueCreateInfo.pQueuePriorities = &queuePriority;

	// Enable features
	VkPhysicalDeviceFeatures deviceFeatures = {};

	VkDeviceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.pQueueCreateInfos = &queueCreateInfo;
	createInfo.queueCreateInfoCount = 1;
	createInfo.pEnabledFeatures = &deviceFeatures;

#ifdef __APPLE__
	// Required extensions for macOS
	const std::vector<const char *> deviceExtensions = {
		VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
		VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME};

	// Check if required extensions are supported
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(_physicalDevice, nullptr, &extensionCount, nullptr);
	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(_physicalDevice, nullptr, &extensionCount, availableExtensions.data());

	std::cout << "Available device extensions:" << std::endl;
	for (const auto &extension : availableExtensions)
	{
		std::cout << "\t" << extension.extensionName << std::endl;
	}

	// Verify all required extensions are available
	for (const auto &requiredExtension : deviceExtensions)
	{
		bool found = false;
		for (const auto &availableExtension : availableExtensions)
		{
			if (strcmp(requiredExtension, availableExtension.extensionName) == 0)
			{
				found = true;
				break;
			}
		}
		if (!found)
		{
			throw std::runtime_error(std::string("Required device extension not available: ") + requiredExtension);
		}
	}

	createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	createInfo.ppEnabledExtensionNames = deviceExtensions.data();
#else
	createInfo.enabledExtensionCount = 0;
#endif

	VkResult result = vkCreateDevice(_physicalDevice, &createInfo, nullptr, &_device);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create logical device! Error code: " + std::to_string(result));
	}

	// Get the graphics queue handle from the device
	vkGetDeviceQueue(_device, indices.graphicsFamily.value(), 0, &_graphicsQueue);
};

bool Application::isDeviceSuitable(const VkPhysicalDevice &device)
{
	// we can query the device properties and features to check if it is suitable
	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(device, &deviceProperties);

	VkPhysicalDeviceFeatures deviceFeatures;
	vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

	QueueFamilyIndices indices = Utils::findQueueFamilyIndex(device);
	if (!indices.isComplete())
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
	if (!isDeviceSuitable(device) || device == VK_NULL_HANDLE)
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
		VK_KHR_SURFACE_EXTENSION_NAME};

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

void Application::checkInstanceExtensionSupport()
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
	if (_instance != nullptr)
	{
		vkDestroyInstance(_instance, nullptr);
	}

	if (_device != nullptr)
	{
		vkDestroyDevice(_device, nullptr);
	}

	if (_window != nullptr)
	{
		glfwDestroyWindow(_window);
		_window = nullptr;
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

		glfwSwapBuffers(_window);
		glfwPollEvents();
	}
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