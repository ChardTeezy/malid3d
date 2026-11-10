#include "vk_probe.h"

#include <windows.h>

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan_core.h>

#include <cstdlib>
#include <cstring>

#include "log.h"

namespace malid3d {

namespace {

struct Vk {
    PFN_vkGetInstanceProcAddr GetInstanceProcAddr = nullptr;
    PFN_vkEnumerateInstanceVersion EnumerateInstanceVersion = nullptr;
    PFN_vkEnumerateInstanceExtensionProperties EnumerateInstanceExtensionProperties = nullptr;
    PFN_vkCreateInstance CreateInstance = nullptr;
    PFN_vkDestroyInstance DestroyInstance = nullptr;
    PFN_vkEnumeratePhysicalDevices EnumeratePhysicalDevices = nullptr;
    PFN_vkGetPhysicalDeviceProperties GetPhysicalDeviceProperties = nullptr;
    PFN_vkGetPhysicalDeviceFeatures GetPhysicalDeviceFeatures = nullptr;
    PFN_vkGetPhysicalDeviceFeatures2 GetPhysicalDeviceFeatures2 = nullptr;
    PFN_vkGetPhysicalDeviceMemoryProperties GetPhysicalDeviceMemoryProperties = nullptr;
    PFN_vkGetPhysicalDeviceFormatProperties GetPhysicalDeviceFormatProperties = nullptr;
    PFN_vkEnumerateDeviceExtensionProperties EnumerateDeviceExtensionProperties = nullptr;
};

template <typename T>
T load_global(const Vk& vk, const char* name) {
    return reinterpret_cast<T>(vk.GetInstanceProcAddr(VK_NULL_HANDLE, name));
}

template <typename T>
T load_inst(const Vk& vk, VkInstance inst, const char* name) {
    return reinterpret_cast<T>(vk.GetInstanceProcAddr(inst, name));
}

int cmp_ext(const void* a, const void* b) {
    return strcmp(static_cast<const VkExtensionProperties*>(a)->extensionName,
                  static_cast<const VkExtensionProperties*>(b)->extensionName);
}

bool has_ext(const VkExtensionProperties* exts, uint32_t count, const char* name) {
    for (uint32_t i = 0; i < count; i++)
        if (strcmp(exts[i].extensionName, name) == 0) return true;
    return false;
}

const char* device_type(VkPhysicalDeviceType t) {
    switch (t) {
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "integrated";
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return "discrete";
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return "virtual";
        case VK_PHYSICAL_DEVICE_TYPE_CPU: return "cpu";
        default: return "other";
    }
}

#define FEAT(s, name) mlog("  %-44s %s", #name, (s).name ? "yes" : "NO")
#define LIMIT_U(l, name) mlog("  %-44s %u", #name, static_cast<unsigned>((l).name))
#define LIMIT_U64(l, name) \
    mlog("  %-44s %llu", #name, static_cast<unsigned long long>((l).name))

// Extensions that matter for translating D3D11. Missing ones are not fatal;
// each needs a fallback path in the layer, so we want to know up front.
const char* const kWantedExts[] = {
    "VK_KHR_swapchain",
    "VK_KHR_maintenance4",
    "VK_KHR_maintenance5",
    "VK_KHR_dynamic_rendering",
    "VK_KHR_dynamic_rendering_local_read",
    "VK_KHR_load_store_op_none",
    "VK_KHR_vertex_attribute_divisor",
    "VK_EXT_vertex_attribute_divisor",
    "VK_EXT_robustness2",
    "VK_EXT_transform_feedback",
    "VK_EXT_depth_clip_enable",
    "VK_EXT_depth_clip_control",
    "VK_EXT_depth_bias_control",
    "VK_EXT_custom_border_color",
    "VK_EXT_border_color_swizzle",
    "VK_EXT_extended_dynamic_state",
    "VK_EXT_extended_dynamic_state2",
    "VK_EXT_extended_dynamic_state3",
    "VK_EXT_graphics_pipeline_library",
    "VK_EXT_conservative_rasterization",
    "VK_EXT_line_rasterization",
    "VK_EXT_provoking_vertex",
    "VK_EXT_non_seamless_cube_map",
    "VK_EXT_shader_stencil_export",
    "VK_EXT_shader_demote_to_helper_invocation",
    "VK_EXT_4444_formats",
    "VK_EXT_memory_budget",
    "VK_EXT_host_image_copy",
    "VK_EXT_attachment_feedback_loop_layout",
    "VK_EXT_multi_draw",
    "VK_EXT_fragment_shader_interlock",
    "VK_EXT_sample_locations",
    "VK_EXT_texture_compression_astc_hdr",
    "VK_ARM_rasterization_order_attachment_access",
    "VK_EXT_rasterization_order_attachment_access",
    "VK_EXT_shader_tile_image",
};

struct FormatProbe {
    VkFormat format;
    const char* name;
};

// Formats D3D11 games lean on. BC* matters most: if the GPU cannot sample
// them we must decompress or transcode textures on the CPU.
const FormatProbe kFormats[] = {
    {VK_FORMAT_B8G8R8A8_UNORM, "B8G8R8A8_UNORM"},
    {VK_FORMAT_B8G8R8A8_SRGB, "B8G8R8A8_SRGB"},
    {VK_FORMAT_R8G8B8A8_UNORM, "R8G8B8A8_UNORM"},
    {VK_FORMAT_B5G6R5_UNORM_PACK16, "B5G6R5_UNORM"},
    {VK_FORMAT_A2B10G10R10_UNORM_PACK32, "A2B10G10R10_UNORM"},
    {VK_FORMAT_B10G11R11_UFLOAT_PACK32, "B10G11R11_UFLOAT"},
    {VK_FORMAT_R16G16B16A16_SFLOAT, "R16G16B16A16_SFLOAT"},
    {VK_FORMAT_R32G32B32A32_SFLOAT, "R32G32B32A32_SFLOAT"},
    {VK_FORMAT_R16G16B16A16_UNORM, "R16G16B16A16_UNORM"},
    {VK_FORMAT_D16_UNORM, "D16_UNORM"},
    {VK_FORMAT_X8_D24_UNORM_PACK32, "X8_D24_UNORM"},
    {VK_FORMAT_D24_UNORM_S8_UINT, "D24_UNORM_S8_UINT"},
    {VK_FORMAT_D32_SFLOAT, "D32_SFLOAT"},
    {VK_FORMAT_D32_SFLOAT_S8_UINT, "D32_SFLOAT_S8_UINT"},
    {VK_FORMAT_BC1_RGBA_UNORM_BLOCK, "BC1_RGBA_UNORM"},
    {VK_FORMAT_BC2_UNORM_BLOCK, "BC2_UNORM"},
    {VK_FORMAT_BC3_UNORM_BLOCK, "BC3_UNORM"},
    {VK_FORMAT_BC4_UNORM_BLOCK, "BC4_UNORM"},
    {VK_FORMAT_BC5_UNORM_BLOCK, "BC5_UNORM"},
    {VK_FORMAT_BC6H_UFLOAT_BLOCK, "BC6H_UFLOAT"},
    {VK_FORMAT_BC7_UNORM_BLOCK, "BC7_UNORM"},
    {VK_FORMAT_ASTC_4x4_UNORM_BLOCK, "ASTC_4x4_UNORM"},
    {VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK, "ETC2_R8G8B8A8_UNORM"},
};

void probe_device(const Vk& vk, VkPhysicalDevice dev, uint32_t index, uint32_t instance_version) {
    VkPhysicalDeviceProperties props;
    memset(&props, 0, sizeof(props));
    vk.GetPhysicalDeviceProperties(dev, &props);

    mlog(" ");
    mlog("==== GPU %u: %s ====", index, props.deviceName);
    mlog("  type            %s", device_type(props.deviceType));
    mlog("  vendorID        0x%04x", props.vendorID);
    mlog("  deviceID        0x%08x", props.deviceID);
    mlog("  apiVersion      %u.%u.%u", VK_API_VERSION_MAJOR(props.apiVersion),
        VK_API_VERSION_MINOR(props.apiVersion), VK_API_VERSION_PATCH(props.apiVersion));
    mlog("  driverVersion   0x%08x (%u.%u.%u)", props.driverVersion,
        VK_API_VERSION_MAJOR(props.driverVersion), VK_API_VERSION_MINOR(props.driverVersion),
        VK_API_VERSION_PATCH(props.driverVersion));

    // ---- memory ----
    VkPhysicalDeviceMemoryProperties mem;
    memset(&mem, 0, sizeof(mem));
    vk.GetPhysicalDeviceMemoryProperties(dev, &mem);
    mlog(" ");
    mlog("-- memory heaps --");
    for (uint32_t i = 0; i < mem.memoryHeapCount; i++) {
        mlog("  heap %u: %llu MiB, flags 0x%x", i,
            static_cast<unsigned long long>(mem.memoryHeaps[i].size >> 20),
            static_cast<unsigned>(mem.memoryHeaps[i].flags));
    }
    for (uint32_t i = 0; i < mem.memoryTypeCount; i++) {
        mlog("  type %u: heap %u, flags 0x%x", i, mem.memoryTypes[i].heapIndex,
            static_cast<unsigned>(mem.memoryTypes[i].propertyFlags));
    }

    // ---- core features ----
    VkPhysicalDeviceFeatures f;
    memset(&f, 0, sizeof(f));
    vk.GetPhysicalDeviceFeatures(dev, &f);
    mlog(" ");
    mlog("-- core features (D3D11-relevant) --");
    FEAT(f, geometryShader);
    FEAT(f, tessellationShader);
    FEAT(f, textureCompressionBC);
    FEAT(f, textureCompressionETC2);
    FEAT(f, textureCompressionASTC_LDR);
    FEAT(f, robustBufferAccess);
    FEAT(f, fullDrawIndexUint32);
    FEAT(f, imageCubeArray);
    FEAT(f, independentBlend);
    FEAT(f, sampleRateShading);
    FEAT(f, dualSrcBlend);
    FEAT(f, logicOp);
    FEAT(f, multiDrawIndirect);
    FEAT(f, drawIndirectFirstInstance);
    FEAT(f, depthClamp);
    FEAT(f, depthBiasClamp);
    FEAT(f, fillModeNonSolid);
    FEAT(f, depthBounds);
    FEAT(f, wideLines);
    FEAT(f, multiViewport);
    FEAT(f, samplerAnisotropy);
    FEAT(f, occlusionQueryPrecise);
    FEAT(f, pipelineStatisticsQuery);
    FEAT(f, vertexPipelineStoresAndAtomics);
    FEAT(f, fragmentStoresAndAtomics);
    FEAT(f, shaderImageGatherExtended);
    FEAT(f, shaderStorageImageExtendedFormats);
    FEAT(f, shaderStorageImageReadWithoutFormat);
    FEAT(f, shaderStorageImageWriteWithoutFormat);
    FEAT(f, shaderClipDistance);
    FEAT(f, shaderCullDistance);
    FEAT(f, shaderFloat64);
    FEAT(f, shaderInt64);
    FEAT(f, shaderInt16);
    FEAT(f, variableMultisampleRate);

    // ---- 1.1 / 1.2 / 1.3 features ----
    const bool has_f2 = vk.GetPhysicalDeviceFeatures2 && instance_version >= VK_API_VERSION_1_1 &&
                        props.apiVersion >= VK_API_VERSION_1_1;
    if (has_f2 && props.apiVersion >= VK_API_VERSION_1_2) {
        VkPhysicalDeviceVulkan13Features f13;
        memset(&f13, 0, sizeof(f13));
        f13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

        VkPhysicalDeviceVulkan12Features f12;
        memset(&f12, 0, sizeof(f12));
        f12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;

        VkPhysicalDeviceVulkan11Features f11;
        memset(&f11, 0, sizeof(f11));
        f11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;

        const bool has_13 = props.apiVersion >= VK_API_VERSION_1_3;
        f11.pNext = &f12;
        f12.pNext = has_13 ? &f13 : nullptr;

        VkPhysicalDeviceFeatures2 f2;
        memset(&f2, 0, sizeof(f2));
        f2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        f2.pNext = &f11;
        vk.GetPhysicalDeviceFeatures2(dev, &f2);

        mlog(" ");
        mlog("-- Vulkan 1.1 features --");
        FEAT(f11, multiview);
        FEAT(f11, shaderDrawParameters);
        FEAT(f11, samplerYcbcrConversion);

        mlog(" ");
        mlog("-- Vulkan 1.2 features --");
        FEAT(f12, drawIndirectCount);
        FEAT(f12, samplerMirrorClampToEdge);
        FEAT(f12, descriptorIndexing);
        FEAT(f12, descriptorBindingPartiallyBound);
        FEAT(f12, descriptorBindingUpdateUnusedWhilePending);
        FEAT(f12, runtimeDescriptorArray);
        FEAT(f12, scalarBlockLayout);
        FEAT(f12, uniformBufferStandardLayout);
        FEAT(f12, imagelessFramebuffer);
        FEAT(f12, separateDepthStencilLayouts);
        FEAT(f12, hostQueryReset);
        FEAT(f12, timelineSemaphore);
        FEAT(f12, bufferDeviceAddress);
        FEAT(f12, shaderOutputViewportIndex);
        FEAT(f12, shaderOutputLayer);
        FEAT(f12, vulkanMemoryModel);

        if (has_13) {
            mlog(" ");
            mlog("-- Vulkan 1.3 features --");
            FEAT(f13, dynamicRendering);
            FEAT(f13, synchronization2);
            FEAT(f13, maintenance4);
            FEAT(f13, shaderDemoteToHelperInvocation);
            FEAT(f13, pipelineCreationCacheControl);
            FEAT(f13, inlineUniformBlock);
            FEAT(f13, subgroupSizeControl);
            FEAT(f13, shaderZeroInitializeWorkgroupMemory);
            FEAT(f13, textureCompressionASTC_HDR);
        }
    } else {
        mlog(" ");
        mlog("-- Vulkan 1.2+ feature query skipped (device apiVersion too low) --");
    }

    // ---- limits ----
    const VkPhysicalDeviceLimits& l = props.limits;
    mlog(" ");
    mlog("-- limits --");
    LIMIT_U(l, maxImageDimension2D);
    LIMIT_U(l, maxImageDimension3D);
    LIMIT_U(l, maxImageDimensionCube);
    LIMIT_U(l, maxImageArrayLayers);
    LIMIT_U(l, maxUniformBufferRange);
    LIMIT_U(l, maxStorageBufferRange);
    LIMIT_U(l, maxPushConstantsSize);
    LIMIT_U(l, maxMemoryAllocationCount);
    LIMIT_U(l, maxSamplerAllocationCount);
    LIMIT_U(l, maxBoundDescriptorSets);
    LIMIT_U(l, maxPerStageDescriptorSamplers);
    LIMIT_U(l, maxPerStageDescriptorUniformBuffers);
    LIMIT_U(l, maxPerStageDescriptorStorageBuffers);
    LIMIT_U(l, maxPerStageDescriptorSampledImages);
    LIMIT_U(l, maxPerStageDescriptorStorageImages);
    LIMIT_U(l, maxPerStageResources);
    LIMIT_U(l, maxDescriptorSetSamplers);
    LIMIT_U(l, maxDescriptorSetUniformBuffers);
    LIMIT_U(l, maxDescriptorSetSampledImages);
    LIMIT_U(l, maxVertexInputAttributes);
    LIMIT_U(l, maxVertexInputBindings);
    LIMIT_U(l, maxVertexOutputComponents);
    LIMIT_U(l, maxFragmentInputComponents);
    LIMIT_U(l, maxFragmentOutputAttachments);
    LIMIT_U(l, maxFragmentDualSrcAttachments);
    LIMIT_U(l, maxColorAttachments);
    LIMIT_U(l, maxViewports);
    LIMIT_U(l, maxClipDistances);
    LIMIT_U(l, maxCullDistances);
    LIMIT_U(l, maxDrawIndexedIndexValue);
    LIMIT_U(l, maxDrawIndirectCount);
    LIMIT_U(l, maxComputeSharedMemorySize);
    LIMIT_U64(l, minUniformBufferOffsetAlignment);
    LIMIT_U64(l, minStorageBufferOffsetAlignment);
    LIMIT_U64(l, minTexelBufferOffsetAlignment);
    LIMIT_U64(l, optimalBufferCopyOffsetAlignment);
    LIMIT_U64(l, nonCoherentAtomSize);
    mlog("  %-44s %.1f", "maxSamplerAnisotropy", static_cast<double>(l.maxSamplerAnisotropy));
    mlog("  %-44s 0x%x", "framebufferColorSampleCounts",
        static_cast<unsigned>(l.framebufferColorSampleCounts));
    mlog("  %-44s 0x%x", "framebufferDepthSampleCounts",
        static_cast<unsigned>(l.framebufferDepthSampleCounts));

    // ---- formats ----
    mlog(" ");
    mlog("-- formats (S=sampled F=filter C=color-target B=blend D=depth-target) --");
    for (const FormatProbe& fp : kFormats) {
        VkFormatProperties p;
        memset(&p, 0, sizeof(p));
        vk.GetPhysicalDeviceFormatProperties(dev, fp.format, &p);
        const VkFormatFeatureFlags o = p.optimalTilingFeatures;
        mlog("  %-24s %c%c%c%c%c  (optimal 0x%08x)", fp.name,
            (o & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) ? 'S' : '-',
            (o & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) ? 'F' : '-',
            (o & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) ? 'C' : '-',
            (o & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT) ? 'B' : '-',
            (o & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) ? 'D' : '-',
            static_cast<unsigned>(o));
    }

    // ---- extensions ----
    uint32_t count = 0;
    VkExtensionProperties* exts = nullptr;
    if (vk.EnumerateDeviceExtensionProperties(dev, nullptr, &count, nullptr) == VK_SUCCESS &&
        count > 0) {
        exts = static_cast<VkExtensionProperties*>(calloc(count, sizeof(VkExtensionProperties)));
        if (exts) {
            VkResult r = vk.EnumerateDeviceExtensionProperties(dev, nullptr, &count, exts);
            if (r != VK_SUCCESS && r != VK_INCOMPLETE) count = 0;
        } else {
            count = 0;
        }
    } else {
        count = 0;
    }

    mlog(" ");
    mlog("-- D3D11-relevant extensions --");
    for (const char* name : kWantedExts)
        mlog("  %-52s %s", name, has_ext(exts, count, name) ? "yes" : "MISSING");

    mlog(" ");
    mlog("-- all device extensions (%u) --", count);
    if (exts) {
        qsort(exts, count, sizeof(VkExtensionProperties), cmp_ext);
        for (uint32_t i = 0; i < count; i++)
            mlog("  %s (v%u)", exts[i].extensionName, exts[i].specVersion);
        free(exts);
    }
}

bool probe() {
    mlog(" ");
    mlog("######## Vulkan probe ########");

    HMODULE lib = LoadLibraryA("vulkan-1.dll");
    if (!lib) {
        mlog("ERROR: could not load vulkan-1.dll (GetLastError=%lu)", GetLastError());
        return false;
    }

    Vk vk;
    vk.GetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
        reinterpret_cast<void*>(GetProcAddress(lib, "vkGetInstanceProcAddr")));
    if (!vk.GetInstanceProcAddr) {
        mlog("ERROR: vulkan-1.dll has no vkGetInstanceProcAddr");
        return false;
    }

    vk.EnumerateInstanceVersion =
        load_global<PFN_vkEnumerateInstanceVersion>(vk, "vkEnumerateInstanceVersion");
    vk.EnumerateInstanceExtensionProperties = load_global<PFN_vkEnumerateInstanceExtensionProperties>(
        vk, "vkEnumerateInstanceExtensionProperties");
    vk.CreateInstance = load_global<PFN_vkCreateInstance>(vk, "vkCreateInstance");
    if (!vk.CreateInstance) {
        mlog("ERROR: vkCreateInstance not found");
        return false;
    }

    uint32_t instance_version = VK_API_VERSION_1_0;
    if (vk.EnumerateInstanceVersion) vk.EnumerateInstanceVersion(&instance_version);
    mlog("instance version: %u.%u.%u", VK_API_VERSION_MAJOR(instance_version),
        VK_API_VERSION_MINOR(instance_version), VK_API_VERSION_PATCH(instance_version));

    if (vk.EnumerateInstanceExtensionProperties) {
        uint32_t n = 0;
        if (vk.EnumerateInstanceExtensionProperties(nullptr, &n, nullptr) == VK_SUCCESS && n > 0) {
            VkExtensionProperties* e =
                static_cast<VkExtensionProperties*>(calloc(n, sizeof(VkExtensionProperties)));
            if (e) {
                VkResult r = vk.EnumerateInstanceExtensionProperties(nullptr, &n, e);
                if (r == VK_SUCCESS || r == VK_INCOMPLETE) {
                    mlog("-- instance extensions (%u) --", n);
                    for (uint32_t i = 0; i < n; i++)
                        mlog("  %s (v%u)", e[i].extensionName, e[i].specVersion);
                }
                free(e);
            }
        }
    }

    VkApplicationInfo app;
    memset(&app, 0, sizeof(app));
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "malid3d-probe";
    app.pEngineName = "malid3d";
    // Ask for the newest version the loader offers, capped at 1.3 (the
    // newest feature structs this probe knows how to read).
    app.apiVersion = instance_version >= VK_API_VERSION_1_3 ? VK_API_VERSION_1_3 : instance_version;

    VkInstanceCreateInfo ici;
    memset(&ici, 0, sizeof(ici));
    ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ici.pApplicationInfo = &app;

    VkInstance inst = VK_NULL_HANDLE;
    VkResult r = vk.CreateInstance(&ici, nullptr, &inst);
    if (r != VK_SUCCESS || !inst) {
        mlog("ERROR: vkCreateInstance failed (VkResult=%d)", static_cast<int>(r));
        return false;
    }

    vk.DestroyInstance = load_inst<PFN_vkDestroyInstance>(vk, inst, "vkDestroyInstance");
    vk.EnumeratePhysicalDevices =
        load_inst<PFN_vkEnumeratePhysicalDevices>(vk, inst, "vkEnumeratePhysicalDevices");
    vk.GetPhysicalDeviceProperties =
        load_inst<PFN_vkGetPhysicalDeviceProperties>(vk, inst, "vkGetPhysicalDeviceProperties");
    vk.GetPhysicalDeviceFeatures =
        load_inst<PFN_vkGetPhysicalDeviceFeatures>(vk, inst, "vkGetPhysicalDeviceFeatures");
    vk.GetPhysicalDeviceFeatures2 =
        load_inst<PFN_vkGetPhysicalDeviceFeatures2>(vk, inst, "vkGetPhysicalDeviceFeatures2");
    vk.GetPhysicalDeviceMemoryProperties = load_inst<PFN_vkGetPhysicalDeviceMemoryProperties>(
        vk, inst, "vkGetPhysicalDeviceMemoryProperties");
    vk.GetPhysicalDeviceFormatProperties = load_inst<PFN_vkGetPhysicalDeviceFormatProperties>(
        vk, inst, "vkGetPhysicalDeviceFormatProperties");
    vk.EnumerateDeviceExtensionProperties = load_inst<PFN_vkEnumerateDeviceExtensionProperties>(
        vk, inst, "vkEnumerateDeviceExtensionProperties");

    bool found = false;
    if (!vk.EnumeratePhysicalDevices || !vk.GetPhysicalDeviceProperties ||
        !vk.GetPhysicalDeviceFeatures || !vk.GetPhysicalDeviceMemoryProperties ||
        !vk.GetPhysicalDeviceFormatProperties || !vk.EnumerateDeviceExtensionProperties) {
        mlog("ERROR: a required Vulkan 1.0 instance function is missing");
    } else {
        uint32_t n = 0;
        r = vk.EnumeratePhysicalDevices(inst, &n, nullptr);
        mlog("physical devices: %u (VkResult=%d)", n, static_cast<int>(r));
        if (r == VK_SUCCESS && n > 0) {
            VkPhysicalDevice devs[8];
            if (n > 8) n = 8;
            r = vk.EnumeratePhysicalDevices(inst, &n, devs);
            if (r == VK_SUCCESS || r == VK_INCOMPLETE) {
                for (uint32_t i = 0; i < n; i++) probe_device(vk, devs[i], i, instance_version);
                found = n > 0;
            }
        }
    }

    if (vk.DestroyInstance) vk.DestroyInstance(inst, nullptr);
    mlog(" ");
    mlog("######## probe finished ########");
    return found;
}

}  // namespace

bool run_vulkan_probe() {
    static LONG started = 0;
    static bool result = false;
    if (InterlockedCompareExchange(&started, 1, 0) == 0) result = probe();
    return result;
}

}  // namespace malid3d
