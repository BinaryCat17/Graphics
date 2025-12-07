#ifndef GLFW_GLFW3_H
#define GLFW_GLFW3_H

#include <stdint.h>
#ifndef VK_HEADER_VERSION
typedef uint64_t VkFlags;
typedef uint64_t VkDeviceSize;
typedef uint64_t VkSampleMask;
typedef uint64_t VkBool32;
typedef uint64_t VkDeviceAddress;
typedef struct VkInstance_T* VkInstance;
typedef struct VkSurfaceKHR_T* VkSurfaceKHR;
typedef struct VkAllocationCallbacks VkAllocationCallbacks;
typedef int32_t VkResult;
#define VK_NULL_HANDLE 0
#define VK_ERROR_INITIALIZATION_FAILED (-3)
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GLFWwindow {
    int placeholder;
} GLFWwindow;

#define GLFW_CLIENT_API 0x00022001
#define GLFW_NO_API 0

int glfwInit(void);
void glfwTerminate(void);
int glfwVulkanSupported(void);
void glfwWindowHint(int hint, int value);
GLFWwindow* glfwCreateWindow(int width, int height, const char* title, GLFWwindow* monitor, GLFWwindow* share);
void glfwDestroyWindow(GLFWwindow* window);
int glfwCreateWindowSurface(VkInstance instance, GLFWwindow* window, const VkAllocationCallbacks* allocator, VkSurfaceKHR* surface);
const char** glfwGetRequiredInstanceExtensions(uint32_t* count);
void glfwGetFramebufferSize(GLFWwindow* window, int* width, int* height);
void glfwWaitEvents(void);
int glfwWindowShouldClose(GLFWwindow* window);
void glfwPollEvents(void);

#ifdef __cplusplus
}
#endif

#endif
