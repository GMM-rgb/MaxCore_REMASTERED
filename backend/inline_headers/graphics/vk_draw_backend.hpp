// =========================================================================
// vk_draw_backend.hpp : DrawBackend over Vulkan 1.1, loaded at run time
// =========================================================================
// libvulkan is opened with dlopen and every function is looked up by name; no
// Vulkan header is included and nothing is linked (struct layouts live in
// vk_types.hpp and are checked against the real vulkan.h by a test).
//
// RENDER TARGET. The 3D scene is rendered OFFSCREEN into a colour image and a
// depth image. Present() then composites the 2D layer and copies the finished
// frame to host memory, and the window shows it with its own per-OS code
// (the ShowFrameFn). This makes the Vulkan path identical on every OS and
// testable without a window system. A swapchain path (no CPU copy) can be
// added per OS behind the same interface.
//
// OWNERSHIP. VulkanDrawBackend owns every Vulkan object it creates and
// destroys them in reverse order of creation in the destructor.
// =========================================================================
#pragma once
#if defined(_WIN32) || defined(_WIN64)
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>
#else
  #include <dlfcn.h>
#endif
#include <cstring>
#include <cstdio>
#include <functional>
#include <vector>
#include <memory>
#include "draw_backend.hpp"
#include "vk_types.hpp"
#include "vk_shaders.hpp"

namespace DrawBackendNS {
namespace VkLoad {
// The Vulkan loader's file name differs per OS. macOS has no Vulkan of its own: it is present only if the
// user installed the Vulkan SDK / MoltenVK, so this simply fails there otherwise and the caller falls back.
inline void* Open() {
#if defined(_WIN32) || defined(_WIN64)
    return reinterpret_cast<void*>(LoadLibraryA("vulkan-1.dll"));
#elif defined(__APPLE__)
    const char* names[] = {"libvulkan.1.dylib", "libvulkan.dylib", "/usr/local/lib/libvulkan.1.dylib", "/opt/homebrew/lib/libvulkan.1.dylib", "libMoltenVK.dylib"};
    for (const char* n : names) if (void* h = dlopen(n, RTLD_NOW | RTLD_LOCAL)) return h;
    return nullptr;
#else
    return dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
#endif
}
inline void* Sym(void* lib, const char* name) {
#if defined(_WIN32) || defined(_WIN64)
    return reinterpret_cast<void*>(GetProcAddress(reinterpret_cast<HMODULE>(lib), name));
#else
    return dlsym(lib, name);
#endif
}
inline void Close(void* lib) {
    if (!lib) return;
#if defined(_WIN32) || defined(_WIN64)
    FreeLibrary(reinterpret_cast<HMODULE>(lib));
#else
    dlclose(lib);
#endif
}
} // namespace VkLoad

using ShowFrameFn = std::function<void(const uint32_t* pixels, int w, int h)>;

class VulkanDrawBackend final : public DrawBackend {
    using Vertex = DrawBackendNS::Vertex;
public:
    static std::unique_ptr<VulkanDrawBackend> Create(int w, int h, ShowFrameFn show, std::string& err) {
        std::unique_ptr<VulkanDrawBackend> be(new VulkanDrawBackend());
        be->show_ = std::move(show);
        if (!be->Init(w, h, err)) return nullptr;      // destructor frees whatever was created
        return be;
    }
    ~VulkanDrawBackend() override { Destroy(); }

    const char* Name() const override { return "vulkan"; }
    std::string Description() const override { return "vulkan: " + deviceName_; }

    void Resize(int w, int h) override {
        if (w <= 0 || h <= 0 || (w == w_ && h == h_)) return;
        WaitIdle();
        DestroyTargets();
        w_ = w; h_ = h;
        std::string err;
        if (!CreateTargets(err)) { std::fprintf(stderr, "[vulkan] resize failed: %s\n", err.c_str()); ok_ = false; }
    }

    void BeginFrame(float r, float g, float b) override {
        clear_[0] = r; clear_[1] = g; clear_[2] = b;
        verts_.clear(); draws_.clear();
    }
    void SetDepthRange(float n, float f) override {
        // The shader reads near/far as constants, so a camera change starts a new batch with its own values.
        near_ = n; far_ = f;
        if (!draws_.empty() && draws_.back().count == 0) { draws_.back().nearP = n; draws_.back().farP = f; return; }
        draws_.push_back({0, 0, 0, n, f});
    }
    void Triangles(const Vertex* v, size_t count, TextureId tex) override {
        if (!v || count == 0 || count % 3 != 0 || !ok_) return;
        if (draws_.empty()) draws_.push_back({0, 0, 0, near_, far_});
        const size_t first = verts_.size();
        verts_.insert(verts_.end(), v, v + count);
        Draw& d = draws_.back();
        if (d.count != 0 && d.tex == tex && d.nearP == near_ && d.farP == far_ && d.first + d.count == first) d.count += count;
        else if (d.count == 0) { d.tex = tex; d.first = first; d.count = count; d.nearP = near_; d.farP = far_; }
        else draws_.push_back({tex, first, count, near_, far_});
    }

    void Present(const uint32_t* canvas, int w, int h, bool has2D) override {
        if (!ok_) return;
        Render();
        std::vector<uint32_t>& px = frame_;
        if (has2D && canvas && w == w_ && h == h_) Composite(canvas, px);
        if (show_) show_(px.data(), w_, h_);
    }

    bool ReadFrame(std::vector<uint32_t>& out) override { out = frame_; return !frame_.empty(); }
    void Shutdown() override { Destroy(); }
    const std::vector<uint32_t>& Frame() const { return frame_; }
    bool Ok() const { return ok_; }

protected:
    TextureId UploadTexture(const Graphics::ImageBuffer& img, const Graphics::TextureBinding& b) override;
    void DeleteTexture(TextureId t) override;

private:
    VulkanDrawBackend() = default;
    struct Draw { TextureId tex; size_t first, count; float nearP, farP; };
    struct Tex { Vk::VkImage image = 0; Vk::VkDeviceMemory mem = 0; Vk::VkImageView view = 0; Vk::VkSampler sampler = 0; Vk::VkDescriptorSet set = 0; };

    bool Init(int w, int h, std::string& err);
    void Destroy();
    bool CreateTargets(std::string& err);
    void DestroyTargets();
    void Render();
    void WaitIdle() { if (dev_ && fnWaitIdle_) fnWaitIdle_(dev_); }
    void Composite(const uint32_t* canvas, std::vector<uint32_t>& px);
    // helpers
    bool FindMemory(uint32_t bits, Vk::VkFlags props, uint32_t& out) const;
    bool MakeBuffer(Vk::VkDeviceSize size, Vk::VkFlags usage, Vk::VkFlags props, Vk::VkBuffer& buf, Vk::VkDeviceMemory& mem, void** mapped, std::string& err);
    bool MakeImage(int w, int h, int format, Vk::VkFlags usage, Vk::VkFlags aspect, Vk::VkImage& img, Vk::VkDeviceMemory& mem, Vk::VkImageView& view, std::string& err);
    Vk::VkCommandBuffer BeginOneShot();
    void EndOneShot(Vk::VkCommandBuffer cb);
    template <class F> F Dev(const char* name) const { return reinterpret_cast<F>(getDevProc_(dev_, name)); }

    ShowFrameFn show_;
    bool ok_ = false;
    int w_ = 0, h_ = 0;
    float clear_[3] = {0.094f, 0.094f, 0.094f}, near_ = 0.1f, far_ = 100.0f;
    std::vector<Vertex> verts_;
    std::vector<Draw> draws_;
    std::vector<uint32_t> frame_;
    std::string deviceName_;
    std::vector<Tex> tex_;      // index = TextureId - 1
    std::vector<Vk::VkBuffer> pendingFree_;

    // Vulkan objects and entry points are defined in the .inl next to this header (kept out of line for readability).
    // ---- loader and objects ----
    void* lib_ = nullptr;
    using GetInstProc = void* (*)(Vk::VkInstance, const char*);
    using GetDevProc = void* (*)(Vk::VkDevice, const char*);
    GetInstProc getInstProc_ = nullptr;
    GetDevProc getDevProc_ = nullptr;
    Vk::VkInstance inst_ = nullptr;
    Vk::VkPhysicalDevice phys_ = nullptr;
    Vk::VkDevice dev_ = nullptr;
    Vk::VkQueue queue_ = nullptr;
    uint32_t family_ = 0;
    Vk::VkPhysicalDeviceMemoryProperties memProps_ = {};

    // entry points (instance level)
    void (*fnDestroyInstance_)(Vk::VkInstance, const void*) = nullptr;
    void (*fnGetMemProps_)(Vk::VkPhysicalDevice, Vk::VkPhysicalDeviceMemoryProperties*) = nullptr;
    // entry points (device level)
    void (*fnDestroyDevice_)(Vk::VkDevice, const void*) = nullptr;
    Vk::VkResult (*fnWaitIdle_)(Vk::VkDevice) = nullptr;
    Vk::VkResult (*fnCreateBuffer_)(Vk::VkDevice, const Vk::VkBufferCreateInfo*, const void*, Vk::VkBuffer*) = nullptr;
    void (*fnDestroyBuffer_)(Vk::VkDevice, Vk::VkBuffer, const void*) = nullptr;
    void (*fnGetBufferReq_)(Vk::VkDevice, Vk::VkBuffer, Vk::VkMemoryRequirements*) = nullptr;
    Vk::VkResult (*fnAllocMem_)(Vk::VkDevice, const Vk::VkMemoryAllocateInfo*, const void*, Vk::VkDeviceMemory*) = nullptr;
    void (*fnFreeMem_)(Vk::VkDevice, Vk::VkDeviceMemory, const void*) = nullptr;
    Vk::VkResult (*fnBindBufMem_)(Vk::VkDevice, Vk::VkBuffer, Vk::VkDeviceMemory, Vk::VkDeviceSize) = nullptr;
    Vk::VkResult (*fnMap_)(Vk::VkDevice, Vk::VkDeviceMemory, Vk::VkDeviceSize, Vk::VkDeviceSize, Vk::VkFlags, void**) = nullptr;
    void (*fnUnmap_)(Vk::VkDevice, Vk::VkDeviceMemory) = nullptr;
    Vk::VkResult (*fnCreateImage_)(Vk::VkDevice, const Vk::VkImageCreateInfo*, const void*, Vk::VkImage*) = nullptr;
    void (*fnDestroyImage_)(Vk::VkDevice, Vk::VkImage, const void*) = nullptr;
    void (*fnGetImageReq_)(Vk::VkDevice, Vk::VkImage, Vk::VkMemoryRequirements*) = nullptr;
    Vk::VkResult (*fnBindImgMem_)(Vk::VkDevice, Vk::VkImage, Vk::VkDeviceMemory, Vk::VkDeviceSize) = nullptr;
    Vk::VkResult (*fnCreateView_)(Vk::VkDevice, const Vk::VkImageViewCreateInfo*, const void*, Vk::VkImageView*) = nullptr;
    void (*fnDestroyView_)(Vk::VkDevice, Vk::VkImageView, const void*) = nullptr;
    Vk::VkResult (*fnCreateShader_)(Vk::VkDevice, const Vk::VkShaderModuleCreateInfo*, const void*, Vk::VkShaderModule*) = nullptr;
    void (*fnDestroyShader_)(Vk::VkDevice, Vk::VkShaderModule, const void*) = nullptr;
    Vk::VkResult (*fnCreateRenderPass_)(Vk::VkDevice, const Vk::VkRenderPassCreateInfo*, const void*, Vk::VkRenderPass*) = nullptr;
    void (*fnDestroyRenderPass_)(Vk::VkDevice, Vk::VkRenderPass, const void*) = nullptr;
    Vk::VkResult (*fnCreateFramebuffer_)(Vk::VkDevice, const Vk::VkFramebufferCreateInfo*, const void*, Vk::VkFramebuffer*) = nullptr;
    void (*fnDestroyFramebuffer_)(Vk::VkDevice, Vk::VkFramebuffer, const void*) = nullptr;
    Vk::VkResult (*fnCreateLayout_)(Vk::VkDevice, const Vk::VkPipelineLayoutCreateInfo*, const void*, Vk::VkPipelineLayout*) = nullptr;
    void (*fnDestroyLayout_)(Vk::VkDevice, Vk::VkPipelineLayout, const void*) = nullptr;
    Vk::VkResult (*fnCreatePipelines_)(Vk::VkDevice, uint64_t, uint32_t, const Vk::VkGraphicsPipelineCreateInfo*, const void*, Vk::VkPipeline*) = nullptr;
    void (*fnDestroyPipeline_)(Vk::VkDevice, Vk::VkPipeline, const void*) = nullptr;
    Vk::VkResult (*fnCreatePool_)(Vk::VkDevice, const Vk::VkCommandPoolCreateInfo*, const void*, Vk::VkCommandPool*) = nullptr;
    void (*fnDestroyPool_)(Vk::VkDevice, Vk::VkCommandPool, const void*) = nullptr;
    Vk::VkResult (*fnAllocCmd_)(Vk::VkDevice, const Vk::VkCommandBufferAllocateInfo*, Vk::VkCommandBuffer*) = nullptr;
    void (*fnFreeCmd_)(Vk::VkDevice, Vk::VkCommandPool, uint32_t, const Vk::VkCommandBuffer*) = nullptr;
    Vk::VkResult (*fnBegin_)(Vk::VkCommandBuffer, const Vk::VkCommandBufferBeginInfo*) = nullptr;
    Vk::VkResult (*fnEnd_)(Vk::VkCommandBuffer) = nullptr;
    void (*fnCmdBeginRP_)(Vk::VkCommandBuffer, const Vk::VkRenderPassBeginInfo*, int32_t) = nullptr;
    void (*fnCmdEndRP_)(Vk::VkCommandBuffer) = nullptr;
    void (*fnCmdBindPipe_)(Vk::VkCommandBuffer, int32_t, Vk::VkPipeline) = nullptr;
    void (*fnCmdBindVB_)(Vk::VkCommandBuffer, uint32_t, uint32_t, const Vk::VkBuffer*, const Vk::VkDeviceSize*) = nullptr;
    void (*fnCmdBindSets_)(Vk::VkCommandBuffer, int32_t, Vk::VkPipelineLayout, uint32_t, uint32_t, const Vk::VkDescriptorSet*, uint32_t, const uint32_t*) = nullptr;
    void (*fnCmdPush_)(Vk::VkCommandBuffer, Vk::VkPipelineLayout, Vk::VkFlags, uint32_t, uint32_t, const void*) = nullptr;
    void (*fnCmdDraw_)(Vk::VkCommandBuffer, uint32_t, uint32_t, uint32_t, uint32_t) = nullptr;
    void (*fnCmdSetViewport_)(Vk::VkCommandBuffer, uint32_t, uint32_t, const Vk::VkViewport*) = nullptr;
    void (*fnCmdSetScissor_)(Vk::VkCommandBuffer, uint32_t, uint32_t, const Vk::VkRect2D*) = nullptr;
    void (*fnCmdCopyImgToBuf_)(Vk::VkCommandBuffer, Vk::VkImage, int32_t, Vk::VkBuffer, uint32_t, const Vk::VkBufferImageCopy*) = nullptr;
    void (*fnCmdCopyBufToImg_)(Vk::VkCommandBuffer, Vk::VkBuffer, Vk::VkImage, int32_t, uint32_t, const Vk::VkBufferImageCopy*) = nullptr;
    void (*fnCmdBarrier_)(Vk::VkCommandBuffer, Vk::VkFlags, Vk::VkFlags, Vk::VkFlags, uint32_t, const void*, uint32_t, const void*, uint32_t, const Vk::VkImageMemoryBarrier*) = nullptr;
    Vk::VkResult (*fnSubmit_)(Vk::VkQueue, uint32_t, const Vk::VkSubmitInfo*, Vk::VkFence) = nullptr;
    Vk::VkResult (*fnQueueIdle_)(Vk::VkQueue) = nullptr;
    Vk::VkResult (*fnCreateFence_)(Vk::VkDevice, const Vk::VkFenceCreateInfo*, const void*, Vk::VkFence*) = nullptr;
    void (*fnDestroyFence_)(Vk::VkDevice, Vk::VkFence, const void*) = nullptr;
    Vk::VkResult (*fnWaitFences_)(Vk::VkDevice, uint32_t, const Vk::VkFence*, Vk::VkBool32, uint64_t) = nullptr;
    Vk::VkResult (*fnResetFences_)(Vk::VkDevice, uint32_t, const Vk::VkFence*) = nullptr;
    Vk::VkResult (*fnCreateSampler_)(Vk::VkDevice, const Vk::VkSamplerCreateInfo*, const void*, Vk::VkSampler*) = nullptr;
    void (*fnDestroySampler_)(Vk::VkDevice, Vk::VkSampler, const void*) = nullptr;
    Vk::VkResult (*fnCreateDSLayout_)(Vk::VkDevice, const Vk::VkDescriptorSetLayoutCreateInfo*, const void*, Vk::VkDescriptorSetLayout*) = nullptr;
    void (*fnDestroyDSLayout_)(Vk::VkDevice, Vk::VkDescriptorSetLayout, const void*) = nullptr;
    Vk::VkResult (*fnCreateDPool_)(Vk::VkDevice, const Vk::VkDescriptorPoolCreateInfo*, const void*, Vk::VkDescriptorPool*) = nullptr;
    void (*fnDestroyDPool_)(Vk::VkDevice, Vk::VkDescriptorPool, const void*) = nullptr;
    Vk::VkResult (*fnAllocDSets_)(Vk::VkDevice, const Vk::VkDescriptorSetAllocateInfo*, Vk::VkDescriptorSet*) = nullptr;
    Vk::VkResult (*fnFreeDSets_)(Vk::VkDevice, Vk::VkDescriptorPool, uint32_t, const Vk::VkDescriptorSet*) = nullptr;
    void (*fnUpdateDSets_)(Vk::VkDevice, uint32_t, const Vk::VkWriteDescriptorSet*, uint32_t, const void*) = nullptr;

    // ---- objects created in Init / CreateTargets ----
    Vk::VkCommandPool pool_ = 0;
    Vk::VkFence fence_ = 0;
    Vk::VkDescriptorSetLayout dsLayout_ = 0;
    Vk::VkDescriptorPool dPool_ = 0;
    uint32_t dPoolCapacity_ = 0, dPoolUsed_ = 0;
    Vk::VkPipelineLayout pipeLayout_ = 0;
    Vk::VkRenderPass renderPass_ = 0;
    Vk::VkPipeline pipeline_ = 0;
    Vk::VkShaderModule vs_ = 0, fs_ = 0;
    Vk::VkImage colorImg_ = 0, depthImg_ = 0;
    Vk::VkDeviceMemory colorMem_ = 0, depthMem_ = 0;
    Vk::VkImageView colorView_ = 0, depthView_ = 0;
    Vk::VkFramebuffer fb_ = 0;
    Vk::VkBuffer readBuf_ = 0, vtxBuf_ = 0;
    Vk::VkDeviceMemory readMem_ = 0, vtxMem_ = 0;
    void* readMap_ = nullptr; void* vtxMap_ = nullptr;
    size_t vtxCapacity_ = 0;
    Vk::VkSampler nearestClamp_ = 0;      // fallback sampler for the white 1x1 texture used by untextured draws
    TextureId whiteTex_ = 0;
};

} // namespace DrawBackendNS

// ---------------------------------------------------------------------
// IMPLEMENTATION
// ---------------------------------------------------------------------
namespace DrawBackendNS {
namespace {
using namespace Vk;
} // helper scope



inline bool VulkanDrawBackend::FindMemory(uint32_t bits, VkFlags props, uint32_t& out) const {
    for (uint32_t i = 0; i < memProps_.memoryTypeCount; ++i)
        if ((bits & (1u << i)) && (memProps_.memoryTypes[i].propertyFlags & props) == props) { out = i; return true; }
    return false;
}

inline bool VulkanDrawBackend::MakeBuffer(VkDeviceSize size, VkFlags usage, VkFlags props, VkBuffer& buf, VkDeviceMemory& mem, void** mapped, std::string& err) {
    VkBufferCreateInfo bi = {ST_BUFFER_CREATE_INFO, nullptr, 0, size, usage, 0, 0, nullptr};
    if (fnCreateBuffer_(dev_, &bi, nullptr, &buf) != VK_SUCCESS) { err = "vkCreateBuffer failed"; return false; }
    VkMemoryRequirements rq; fnGetBufferReq_(dev_, buf, &rq);
    uint32_t type; if (!FindMemory(rq.memoryTypeBits, props, type)) { err = "no suitable memory type"; fnDestroyBuffer_(dev_, buf, nullptr); buf = 0; return false; }
    VkMemoryAllocateInfo ai = {ST_MEMORY_ALLOCATE_INFO, nullptr, rq.size, type};
    if (fnAllocMem_(dev_, &ai, nullptr, &mem) != VK_SUCCESS) { err = "vkAllocateMemory failed"; fnDestroyBuffer_(dev_, buf, nullptr); buf = 0; return false; }
    fnBindBufMem_(dev_, buf, mem, 0);
    if (mapped && fnMap_(dev_, mem, 0, size, 0, mapped) != VK_SUCCESS) { err = "vkMapMemory failed"; return false; }
    return true;
}

inline bool VulkanDrawBackend::MakeImage(int w, int h, int format, VkFlags usage, VkFlags aspect, VkImage& img, VkDeviceMemory& mem, VkImageView& view, std::string& err) {
    VkImageCreateInfo ii = {ST_IMAGE_CREATE_INFO, nullptr, 0, 1 /*2D*/, format, {(uint32_t)w, (uint32_t)h, 1}, 1, 1, 1 /*1 sample*/, 0 /*optimal*/, usage, 0, 0, nullptr, LAYOUT_UNDEFINED};
    if (fnCreateImage_(dev_, &ii, nullptr, &img) != VK_SUCCESS) { err = "vkCreateImage failed"; return false; }
    VkMemoryRequirements rq; fnGetImageReq_(dev_, img, &rq);
    uint32_t type; if (!FindMemory(rq.memoryTypeBits, MEM_DEVICE_LOCAL, type) && !FindMemory(rq.memoryTypeBits, 0, type)) { err = "no memory type for image"; return false; }
    VkMemoryAllocateInfo ai = {ST_MEMORY_ALLOCATE_INFO, nullptr, rq.size, type};
    if (fnAllocMem_(dev_, &ai, nullptr, &mem) != VK_SUCCESS) { err = "vkAllocateMemory (image) failed"; return false; }
    fnBindImgMem_(dev_, img, mem, 0);
    VkImageViewCreateInfo vi = {ST_IMAGE_VIEW_CREATE_INFO, nullptr, 0, img, 1 /*2D*/, format, {0, 0, 0, 0}, {aspect, 0, 1, 0, 1}};
    if (fnCreateView_(dev_, &vi, nullptr, &view) != VK_SUCCESS) { err = "vkCreateImageView failed"; return false; }
    return true;
}

inline VkCommandBuffer VulkanDrawBackend::BeginOneShot() {
    VkCommandBufferAllocateInfo ai = {ST_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, pool_, 0 /*primary*/, 1};
    VkCommandBuffer cb = nullptr; if (fnAllocCmd_(dev_, &ai, &cb) != VK_SUCCESS) return nullptr;
    VkCommandBufferBeginInfo bi = {ST_COMMAND_BUFFER_BEGIN_INFO, nullptr, CMD_ONE_TIME_SUBMIT, nullptr};
    fnBegin_(cb, &bi);
    return cb;
}
inline void VulkanDrawBackend::EndOneShot(VkCommandBuffer cb) {
    if (!cb) return;
    fnEnd_(cb);
    VkSubmitInfo si = {ST_SUBMIT_INFO, nullptr, 0, nullptr, nullptr, 1, &cb, 0, nullptr};
    fnSubmit_(queue_, 1, &si, 0);
    fnQueueIdle_(queue_);
    fnFreeCmd_(dev_, pool_, 1, &cb);
}

inline bool VulkanDrawBackend::Init(int w, int h, std::string& err) {
    w_ = w; h_ = h;
    lib_ = VkLoad::Open();
    if (!lib_) { err = "the Vulkan loader was not found on this machine"; return false; }
    getInstProc_ = reinterpret_cast<GetInstProc>(VkLoad::Sym(lib_, "vkGetInstanceProcAddr"));
    if (!getInstProc_) { err = "vkGetInstanceProcAddr missing"; return false; }

    auto createInstance = reinterpret_cast<VkResult (*)(const VkInstanceCreateInfo*, const void*, VkInstance*)>(getInstProc_(nullptr, "vkCreateInstance"));
    if (!createInstance) { err = "vkCreateInstance missing"; return false; }
    VkApplicationInfo app = {ST_APPLICATION_INFO, nullptr, "MaxCore", 1, "MaxCore", 1, (1u << 22) | (1u << 12)};
    VkInstanceCreateInfo ci = {ST_INSTANCE_CREATE_INFO, nullptr, 0, &app, 0, nullptr, 0, nullptr};
    if (createInstance(&ci, nullptr, &inst_) != VK_SUCCESS) { err = "vkCreateInstance failed"; return false; }
    fnDestroyInstance_ = reinterpret_cast<void (*)(VkInstance, const void*)>(getInstProc_(inst_, "vkDestroyInstance"));

    auto enumDev = reinterpret_cast<VkResult (*)(VkInstance, uint32_t*, VkPhysicalDevice*)>(getInstProc_(inst_, "vkEnumeratePhysicalDevices"));
    uint32_t n = 0; enumDev(inst_, &n, nullptr);
    if (n == 0) { err = "no Vulkan device"; return false; }
    std::vector<VkPhysicalDevice> devs(n); enumDev(inst_, &n, devs.data());
    // Prefer a real GPU (discrete 2, integrated 1) over a software device (4), and either over "other".
    struct Props { uint32_t apiVersion, driverVersion, vendorID, deviceID; int deviceType; char name[256]; uint8_t rest[1024]; };
    auto getProps = reinterpret_cast<void (*)(VkPhysicalDevice, Props*)>(getInstProc_(inst_, "vkGetPhysicalDeviceProperties"));
    auto getQF = reinterpret_cast<void (*)(VkPhysicalDevice, uint32_t*, VkQueueFamilyProperties*)>(getInstProc_(inst_, "vkGetPhysicalDeviceQueueFamilyProperties"));
    int bestScore = -1;
    for (auto d : devs) {
        Props p; getProps(d, &p);
        uint32_t nq = 0; getQF(d, &nq, nullptr); std::vector<VkQueueFamilyProperties> q(nq); getQF(d, &nq, q.data());
        int fam = -1; for (uint32_t i = 0; i < nq; ++i) if (q[i].queueFlags & QUEUE_GRAPHICS) { fam = (int)i; break; }
        if (fam < 0) continue;
        int score = p.deviceType == 2 ? 3 : p.deviceType == 1 ? 2 : p.deviceType == 4 ? 0 : 1;
        if (score > bestScore) { bestScore = score; phys_ = d; family_ = (uint32_t)fam; deviceName_ = p.name; }
    }
    if (!phys_) { err = "no device with a graphics queue"; return false; }
    fnGetMemProps_ = reinterpret_cast<void (*)(VkPhysicalDevice, VkPhysicalDeviceMemoryProperties*)>(getInstProc_(inst_, "vkGetPhysicalDeviceMemoryProperties"));
    fnGetMemProps_(phys_, &memProps_);

    auto createDevice = reinterpret_cast<VkResult (*)(VkPhysicalDevice, const VkDeviceCreateInfo*, const void*, VkDevice*)>(getInstProc_(inst_, "vkCreateDevice"));
    getDevProc_ = reinterpret_cast<GetDevProc>(getInstProc_(inst_, "vkGetDeviceProcAddr"));
    float prio = 1.0f;
    VkDeviceQueueCreateInfo qci = {ST_DEVICE_QUEUE_CREATE_INFO, nullptr, 0, family_, 1, &prio};
    VkDeviceCreateInfo dci = {ST_DEVICE_CREATE_INFO, nullptr, 0, 1, &qci, 0, nullptr, 0, nullptr, nullptr};
    if (createDevice(phys_, &dci, nullptr, &dev_) != VK_SUCCESS) { err = "vkCreateDevice failed"; return false; }

#define VKLOAD(member, name) do { member = Dev<decltype(member)>(name); if (!member) { err = std::string("missing ") + name; return false; } } while (0)
    VKLOAD(fnDestroyDevice_, "vkDestroyDevice"); VKLOAD(fnWaitIdle_, "vkDeviceWaitIdle");
    VKLOAD(fnCreateBuffer_, "vkCreateBuffer"); VKLOAD(fnDestroyBuffer_, "vkDestroyBuffer"); VKLOAD(fnGetBufferReq_, "vkGetBufferMemoryRequirements");
    VKLOAD(fnAllocMem_, "vkAllocateMemory"); VKLOAD(fnFreeMem_, "vkFreeMemory"); VKLOAD(fnBindBufMem_, "vkBindBufferMemory"); VKLOAD(fnMap_, "vkMapMemory"); VKLOAD(fnUnmap_, "vkUnmapMemory");
    VKLOAD(fnCreateImage_, "vkCreateImage"); VKLOAD(fnDestroyImage_, "vkDestroyImage"); VKLOAD(fnGetImageReq_, "vkGetImageMemoryRequirements"); VKLOAD(fnBindImgMem_, "vkBindImageMemory");
    VKLOAD(fnCreateView_, "vkCreateImageView"); VKLOAD(fnDestroyView_, "vkDestroyImageView"); VKLOAD(fnCreateShader_, "vkCreateShaderModule"); VKLOAD(fnDestroyShader_, "vkDestroyShaderModule");
    VKLOAD(fnCreateRenderPass_, "vkCreateRenderPass"); VKLOAD(fnDestroyRenderPass_, "vkDestroyRenderPass"); VKLOAD(fnCreateFramebuffer_, "vkCreateFramebuffer"); VKLOAD(fnDestroyFramebuffer_, "vkDestroyFramebuffer");
    VKLOAD(fnCreateLayout_, "vkCreatePipelineLayout"); VKLOAD(fnDestroyLayout_, "vkDestroyPipelineLayout"); VKLOAD(fnCreatePipelines_, "vkCreateGraphicsPipelines"); VKLOAD(fnDestroyPipeline_, "vkDestroyPipeline");
    VKLOAD(fnCreatePool_, "vkCreateCommandPool"); VKLOAD(fnDestroyPool_, "vkDestroyCommandPool"); VKLOAD(fnAllocCmd_, "vkAllocateCommandBuffers"); VKLOAD(fnFreeCmd_, "vkFreeCommandBuffers");
    VKLOAD(fnBegin_, "vkBeginCommandBuffer"); VKLOAD(fnEnd_, "vkEndCommandBuffer"); VKLOAD(fnCmdBeginRP_, "vkCmdBeginRenderPass"); VKLOAD(fnCmdEndRP_, "vkCmdEndRenderPass");
    VKLOAD(fnCmdBindPipe_, "vkCmdBindPipeline"); VKLOAD(fnCmdBindVB_, "vkCmdBindVertexBuffers"); VKLOAD(fnCmdBindSets_, "vkCmdBindDescriptorSets"); VKLOAD(fnCmdPush_, "vkCmdPushConstants");
    VKLOAD(fnCmdDraw_, "vkCmdDraw"); VKLOAD(fnCmdSetViewport_, "vkCmdSetViewport"); VKLOAD(fnCmdSetScissor_, "vkCmdSetScissor"); VKLOAD(fnCmdCopyImgToBuf_, "vkCmdCopyImageToBuffer");
    VKLOAD(fnCmdCopyBufToImg_, "vkCmdCopyBufferToImage"); VKLOAD(fnCmdBarrier_, "vkCmdPipelineBarrier"); VKLOAD(fnSubmit_, "vkQueueSubmit"); VKLOAD(fnQueueIdle_, "vkQueueWaitIdle");
    VKLOAD(fnCreateFence_, "vkCreateFence"); VKLOAD(fnDestroyFence_, "vkDestroyFence"); VKLOAD(fnWaitFences_, "vkWaitForFences"); VKLOAD(fnResetFences_, "vkResetFences");
    VKLOAD(fnCreateSampler_, "vkCreateSampler"); VKLOAD(fnDestroySampler_, "vkDestroySampler"); VKLOAD(fnCreateDSLayout_, "vkCreateDescriptorSetLayout"); VKLOAD(fnDestroyDSLayout_, "vkDestroyDescriptorSetLayout");
    VKLOAD(fnCreateDPool_, "vkCreateDescriptorPool"); VKLOAD(fnDestroyDPool_, "vkDestroyDescriptorPool"); VKLOAD(fnAllocDSets_, "vkAllocateDescriptorSets"); VKLOAD(fnFreeDSets_, "vkFreeDescriptorSets"); VKLOAD(fnUpdateDSets_, "vkUpdateDescriptorSets");
#undef VKLOAD
    auto getQueue = Dev<void (*)(VkDevice, uint32_t, uint32_t, VkQueue*)>("vkGetDeviceQueue");
    if (!getQueue) { err = "missing vkGetDeviceQueue"; return false; }
    getQueue(dev_, family_, 0, &queue_);

    VkCommandPoolCreateInfo pci = {ST_COMMAND_POOL_CREATE_INFO, nullptr, COMMAND_POOL_RESET_BUFFER, family_};
    if (fnCreatePool_(dev_, &pci, nullptr, &pool_) != VK_SUCCESS) { err = "vkCreateCommandPool failed"; return false; }
    VkFenceCreateInfo fi = {ST_FENCE_CREATE_INFO, nullptr, 0};
    if (fnCreateFence_(dev_, &fi, nullptr, &fence_) != VK_SUCCESS) { err = "vkCreateFence failed"; return false; }

    // descriptor set layout: one combined image sampler (the texture), fragment stage
    VkDescriptorSetLayoutBinding b0 = {0, 1 /*COMBINED_IMAGE_SAMPLER*/, 1, STAGE_FRAGMENT, nullptr};
    VkDescriptorSetLayoutCreateInfo dli = {ST_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0, 1, &b0};
    if (fnCreateDSLayout_(dev_, &dli, nullptr, &dsLayout_) != VK_SUCCESS) { err = "descriptor set layout failed"; return false; }
    VkPushConstantRange pcr = {STAGE_VERTEX, 0, 16};      // viewport (2 floats) + near/far (2 floats)
    VkPipelineLayoutCreateInfo pli = {ST_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 1, &dsLayout_, 1, &pcr};
    if (fnCreateLayout_(dev_, &pli, nullptr, &pipeLayout_) != VK_SUCCESS) { err = "pipeline layout failed"; return false; }

    if (!CreateTargets(err)) return false;

    // a 1x1 white texture, bound for flat-colour draws (the shader always has a sampler)
    Graphics::ImageBuffer white; white.width = 1; white.height = 1; white.pixels.assign(1, 0xFFFFFFFFu);
    Graphics::TextureBinding wb; wb.filter = Graphics::TextureFilter::Nearest; wb.wrap = Graphics::TextureWrap::Clamp;
    whiteTex_ = UploadTexture(white, wb);
    if (!whiteTex_) { err = "could not create the placeholder texture"; return false; }
    ok_ = true;
    return true;
}

inline bool VulkanDrawBackend::CreateTargets(std::string& err) {
    if (!MakeImage(w_, h_, FORMAT_B8G8R8A8_UNORM, IMAGE_COLOR_ATTACHMENT | IMAGE_TRANSFER_SRC, ASPECT_COLOR, colorImg_, colorMem_, colorView_, err)) return false;
    if (!MakeImage(w_, h_, FORMAT_D32_SFLOAT, IMAGE_DEPTH_STENCIL, ASPECT_DEPTH, depthImg_, depthMem_, depthView_, err)) return false;
    if (!renderPass_) {
        VkAttachmentDescription att[2] = {
            {0, FORMAT_B8G8R8A8_UNORM, 1, LOAD_OP_CLEAR, STORE_OP_STORE, LOAD_OP_DONT_CARE, STORE_OP_DONT_CARE, LAYOUT_UNDEFINED, LAYOUT_TRANSFER_SRC},
            {0, FORMAT_D32_SFLOAT, 1, LOAD_OP_CLEAR, STORE_OP_DONT_CARE, LOAD_OP_DONT_CARE, STORE_OP_DONT_CARE, LAYOUT_UNDEFINED, LAYOUT_DEPTH_STENCIL_ATTACHMENT}};
        VkAttachmentReference cref = {0, LAYOUT_COLOR_ATTACHMENT}, dref = {1, LAYOUT_DEPTH_STENCIL_ATTACHMENT};
        VkSubpassDescription sp = {0, 0 /*graphics*/, 0, nullptr, 1, &cref, nullptr, &dref, 0, nullptr};
        VkSubpassDependency dep = {0xFFFFFFFFu /*EXTERNAL*/, 0, STAGE_COLOR_OUTPUT, STAGE_COLOR_OUTPUT, 0, ACCESS_COLOR_WRITE, 0};
        VkRenderPassCreateInfo rpi = {ST_RENDER_PASS_CREATE_INFO, nullptr, 0, 2, att, 1, &sp, 1, &dep};
        if (fnCreateRenderPass_(dev_, &rpi, nullptr, &renderPass_) != VK_SUCCESS) { err = "vkCreateRenderPass failed"; return false; }
    }
    VkImageView views[2] = {colorView_, depthView_};
    VkFramebufferCreateInfo fbi = {ST_FRAMEBUFFER_CREATE_INFO, nullptr, 0, renderPass_, 2, views, (uint32_t)w_, (uint32_t)h_, 1};
    if (fnCreateFramebuffer_(dev_, &fbi, nullptr, &fb_) != VK_SUCCESS) { err = "vkCreateFramebuffer failed"; return false; }
    if (!MakeBuffer((VkDeviceSize)w_ * h_ * 4, BUFFER_TRANSFER_DST, MEM_HOST_VISIBLE | MEM_HOST_COHERENT, readBuf_, readMem_, &readMap_, err)) return false;
    frame_.assign((size_t)w_ * h_, 0);

    if (!pipeline_) {
        VkShaderModuleCreateInfo vsi = {ST_SHADER_MODULE_CREATE_INFO, nullptr, 0, VulkanShaders::kSceneVertWords * 4, VulkanShaders::kSceneVert};
        VkShaderModuleCreateInfo fsi = {ST_SHADER_MODULE_CREATE_INFO, nullptr, 0, VulkanShaders::kSceneFragWords * 4, VulkanShaders::kSceneFrag};
        if (fnCreateShader_(dev_, &vsi, nullptr, &vs_) != VK_SUCCESS || fnCreateShader_(dev_, &fsi, nullptr, &fs_) != VK_SUCCESS) { err = "shader module failed"; return false; }
        VkPipelineShaderStageCreateInfo stages[2] = {{ST_PIPELINE_SHADER_STAGE, nullptr, 0, STAGE_VERTEX, vs_, "main", nullptr}, {ST_PIPELINE_SHADER_STAGE, nullptr, 0, STAGE_FRAGMENT, fs_, "main", nullptr}};
        VkVertexInputBindingDescription bind = {0, (uint32_t)sizeof(Vertex), 0};
        VkVertexInputAttributeDescription attrs[5] = {
            {0, 0, FORMAT_R32G32B32_SFLOAT, (uint32_t)offsetof(Vertex, x)}, {1, 0, FORMAT_R32_SFLOAT, (uint32_t)offsetof(Vertex, invW)}, {2, 0, FORMAT_R32G32_SFLOAT, (uint32_t)offsetof(Vertex, u)},
            {3, 0, FORMAT_R32G32B32A32_SFLOAT, (uint32_t)offsetof(Vertex, r)}, {4, 0, FORMAT_R32_SFLOAT, (uint32_t)offsetof(Vertex, texMix)}};
        VkPipelineVertexInputStateCreateInfo vin = {ST_PIPELINE_VERTEX_INPUT, nullptr, 0, 1, &bind, 5, attrs};
        VkPipelineInputAssemblyStateCreateInfo ia = {ST_PIPELINE_INPUT_ASSEMBLY, nullptr, 0, 3 /*TRIANGLE_LIST*/, 0};
        VkPipelineViewportStateCreateInfo vp = {ST_PIPELINE_VIEWPORT, nullptr, 0, 1, nullptr, 1, nullptr};      // both dynamic
        VkPipelineRasterizationStateCreateInfo rs = {ST_PIPELINE_RASTERIZATION, nullptr, 0, 0, 0, 0 /*fill*/, 0 /*no culling*/, 0, 0, 0, 0, 0, 1.0f};
        VkPipelineMultisampleStateCreateInfo ms = {ST_PIPELINE_MULTISAMPLE, nullptr, 0, 1, 0, 0, nullptr, 0, 0};
        VkPipelineDepthStencilStateCreateInfo ds = {}; ds.sType = ST_PIPELINE_DEPTH_STENCIL; ds.depthTestEnable = 1; ds.depthWriteEnable = 1; ds.depthCompareOp = 1 /*LESS*/; ds.maxDepthBounds = 1.0f;
        VkPipelineColorBlendAttachmentState cba = {1, 6 /*SRC_ALPHA*/, 7 /*ONE_MINUS_SRC_ALPHA*/, 0 /*ADD*/, 1 /*ONE*/, 7, 0, COLOR_WRITE_ALL};
        VkPipelineColorBlendStateCreateInfo cb = {ST_PIPELINE_COLOR_BLEND, nullptr, 0, 0, 0, 1, &cba, {0, 0, 0, 0}};
        int32_t dyn[2] = {0 /*VIEWPORT*/, 1 /*SCISSOR*/};
        VkPipelineDynamicStateCreateInfo dsi = {ST_PIPELINE_DYNAMIC_STATE, nullptr, 0, 2, dyn};
        VkGraphicsPipelineCreateInfo gpi = {ST_GRAPHICS_PIPELINE_CREATE_INFO, nullptr, 0, 2, stages, &vin, &ia, nullptr, &vp, &rs, &ms, &ds, &cb, &dsi, pipeLayout_, renderPass_, 0, 0, -1};
        if (fnCreatePipelines_(dev_, 0, 1, &gpi, nullptr, &pipeline_) != VK_SUCCESS) { err = "vkCreateGraphicsPipelines failed"; return false; }
    }
    return true;
}

inline void VulkanDrawBackend::DestroyTargets() {
    if (!dev_) return;
    if (readMap_) { fnUnmap_(dev_, readMem_); readMap_ = nullptr; }
    if (readBuf_) { fnDestroyBuffer_(dev_, readBuf_, nullptr); readBuf_ = 0; }
    if (readMem_) { fnFreeMem_(dev_, readMem_, nullptr); readMem_ = 0; }
    if (fb_) { fnDestroyFramebuffer_(dev_, fb_, nullptr); fb_ = 0; }
    if (colorView_) { fnDestroyView_(dev_, colorView_, nullptr); colorView_ = 0; }
    if (depthView_) { fnDestroyView_(dev_, depthView_, nullptr); depthView_ = 0; }
    if (colorImg_) { fnDestroyImage_(dev_, colorImg_, nullptr); colorImg_ = 0; }
    if (depthImg_) { fnDestroyImage_(dev_, depthImg_, nullptr); depthImg_ = 0; }
    if (colorMem_) { fnFreeMem_(dev_, colorMem_, nullptr); colorMem_ = 0; }
    if (depthMem_) { fnFreeMem_(dev_, depthMem_, nullptr); depthMem_ = 0; }
}

inline void VulkanDrawBackend::Destroy() {
    if (!dev_) {
        // Init failed before a device existed (or Destroy already ran): release what is left, exactly once.
        if (inst_ && fnDestroyInstance_) fnDestroyInstance_(inst_, nullptr);
        inst_ = nullptr;
        if (lib_) VkLoad::Close(lib_);
        lib_ = nullptr;
        return;
    }
    WaitIdle();
    DropAllTextures();                                   // cached textures (frees their GPU objects)
    for (TextureId t = 1; t <= tex_.size(); ++t) if (tex_[t - 1].image) DeleteTexture(t);     // placeholder and any strays
    DestroyTargets();
    if (vtxMap_) { fnUnmap_(dev_, vtxMem_); vtxMap_ = nullptr; }
    if (vtxBuf_) fnDestroyBuffer_(dev_, vtxBuf_, nullptr);
    if (vtxMem_) fnFreeMem_(dev_, vtxMem_, nullptr);
    if (pipeline_) fnDestroyPipeline_(dev_, pipeline_, nullptr);
    if (vs_) fnDestroyShader_(dev_, vs_, nullptr);
    if (fs_) fnDestroyShader_(dev_, fs_, nullptr);
    if (renderPass_) fnDestroyRenderPass_(dev_, renderPass_, nullptr);
    if (pipeLayout_) fnDestroyLayout_(dev_, pipeLayout_, nullptr);
    if (dPool_) fnDestroyDPool_(dev_, dPool_, nullptr);
    if (dsLayout_) fnDestroyDSLayout_(dev_, dsLayout_, nullptr);
    if (fence_) fnDestroyFence_(dev_, fence_, nullptr);
    if (pool_) fnDestroyPool_(dev_, pool_, nullptr);
    fnDestroyDevice_(dev_, nullptr);
    if (inst_ && fnDestroyInstance_) fnDestroyInstance_(inst_, nullptr);
    if (lib_) VkLoad::Close(lib_);
    dev_ = nullptr; inst_ = nullptr; lib_ = nullptr; ok_ = false;
}

inline TextureId VulkanDrawBackend::UploadTexture(const Graphics::ImageBuffer& img, const Graphics::TextureBinding& b) {
    if (img.width <= 0 || img.height <= 0 || img.pixels.size() < (size_t)img.width * img.height) return 0;
    std::string err;
    // Descriptor sets come from a pool that grows by replacing it with a bigger one is NOT safe once sets exist, so
    // allocate generously up front and fail cleanly (return 0) if it is exhausted.
    if (!dPool_) {
        dPoolCapacity_ = 4096;
        VkDescriptorPoolSize ps = {1, dPoolCapacity_};
        VkDescriptorPoolCreateInfo pi = {ST_DESCRIPTOR_POOL_CREATE_INFO, nullptr, 0x1 /*FREE_DESCRIPTOR_SET*/, dPoolCapacity_, 1, &ps};
        if (fnCreateDPool_(dev_, &pi, nullptr, &dPool_) != VK_SUCCESS) return 0;
    }
    Tex t;
    // staging buffer with the pixels (our AARRGGBB uint32 is B,G,R,A in memory = VK_FORMAT_B8G8R8A8)
    VkBuffer stage = 0; VkDeviceMemory stageMem = 0; void* map = nullptr;
    const VkDeviceSize bytes = (VkDeviceSize)img.width * img.height * 4;
    if (!MakeBuffer(bytes, BUFFER_TRANSFER_SRC, MEM_HOST_VISIBLE | MEM_HOST_COHERENT, stage, stageMem, &map, err)) { if (stageMem) fnFreeMem_(dev_, stageMem, nullptr); return 0; }
    std::memcpy(map, img.pixels.data(), (size_t)bytes);
    fnUnmap_(dev_, stageMem);

    bool ok = MakeImage(img.width, img.height, FORMAT_B8G8R8A8_UNORM, IMAGE_TRANSFER_DST | IMAGE_SAMPLED, ASPECT_COLOR, t.image, t.mem, t.view, err);
    if (ok) {
        VkCommandBuffer cb = BeginOneShot();
        VkImageSubresourceRange rng = {ASPECT_COLOR, 0, 1, 0, 1};
        VkImageMemoryBarrier toDst = {ST_IMAGE_MEMORY_BARRIER, nullptr, 0, ACCESS_TRANSFER_WRITE, LAYOUT_UNDEFINED, LAYOUT_TRANSFER_DST, 0xFFFFFFFFu, 0xFFFFFFFFu, t.image, rng};
        fnCmdBarrier_(cb, STAGE_TOP_OF_PIPE, STAGE_TRANSFER, 0, 0, nullptr, 0, nullptr, 1, &toDst);
        VkBufferImageCopy cp = {0, 0, 0, {ASPECT_COLOR, 0, 0, 1}, {0, 0, 0}, {(uint32_t)img.width, (uint32_t)img.height, 1}};
        fnCmdCopyBufToImg_(cb, stage, t.image, LAYOUT_TRANSFER_DST, 1, &cp);
        VkImageMemoryBarrier toRead = {ST_IMAGE_MEMORY_BARRIER, nullptr, ACCESS_TRANSFER_WRITE, ACCESS_SHADER_READ, LAYOUT_TRANSFER_DST, LAYOUT_SHADER_READ_ONLY, 0xFFFFFFFFu, 0xFFFFFFFFu, t.image, rng};
        fnCmdBarrier_(cb, STAGE_TRANSFER, STAGE_FRAGMENT, 0, 0, nullptr, 0, nullptr, 1, &toRead);
        EndOneShot(cb);
    }
    fnDestroyBuffer_(dev_, stage, nullptr); fnFreeMem_(dev_, stageMem, nullptr);

    if (ok) {
        const int filter = b.filter == Graphics::TextureFilter::Bilinear ? 1 : 0;
        const int addr = b.wrap == Graphics::TextureWrap::Repeat ? 0 : b.wrap == Graphics::TextureWrap::Mirror ? 1 : 2;      // REPEAT, MIRRORED_REPEAT, CLAMP_TO_EDGE
        VkSamplerCreateInfo sci = {ST_SAMPLER_CREATE_INFO, nullptr, 0, filter, filter, 0, addr, addr, addr, 0.0f, 0, 1.0f, 0, 0, 0.0f, 0.0f, 0, 0};
        ok = fnCreateSampler_(dev_, &sci, nullptr, &t.sampler) == VK_SUCCESS;
    }
    if (ok && dPoolUsed_ >= dPoolCapacity_) ok = false;
    if (ok) {
        VkDescriptorSetAllocateInfo ai = {ST_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, dPool_, 1, &dsLayout_};
        ok = fnAllocDSets_(dev_, &ai, &t.set) == VK_SUCCESS;
        if (ok) ++dPoolUsed_;
    }
    if (ok) {
        VkDescriptorImageInfo ii = {t.sampler, t.view, LAYOUT_SHADER_READ_ONLY};
        VkWriteDescriptorSet w = {ST_WRITE_DESCRIPTOR_SET, nullptr, t.set, 0, 0, 1, 1, &ii, nullptr, nullptr};
        fnUpdateDSets_(dev_, 1, &w, 0, nullptr);
    }
    if (!ok) {
        if (t.sampler) fnDestroySampler_(dev_, t.sampler, nullptr);
        if (t.view) fnDestroyView_(dev_, t.view, nullptr);
        if (t.image) fnDestroyImage_(dev_, t.image, nullptr);
        if (t.mem) fnFreeMem_(dev_, t.mem, nullptr);
        return 0;
    }
    // reuse a freed slot if there is one so ids do not grow forever
    for (size_t i = 0; i < tex_.size(); ++i) if (!tex_[i].image) { tex_[i] = t; return (TextureId)(i + 1); }
    tex_.push_back(t);
    return (TextureId)tex_.size();
}

inline void VulkanDrawBackend::DeleteTexture(TextureId id) {
    if (!id || id > tex_.size() || !tex_[id - 1].image) return;
    Tex& t = tex_[id - 1];
    WaitIdle();                                           // no frame may still be reading it
    if (t.set && dPool_) { fnFreeDSets_(dev_, dPool_, 1, &t.set); if (dPoolUsed_) --dPoolUsed_; }
    if (t.sampler) fnDestroySampler_(dev_, t.sampler, nullptr);
    if (t.view) fnDestroyView_(dev_, t.view, nullptr);
    if (t.image) fnDestroyImage_(dev_, t.image, nullptr);
    if (t.mem) fnFreeMem_(dev_, t.mem, nullptr);
    t = Tex();
}

inline void VulkanDrawBackend::Render() {
    // ---- vertex buffer: grows geometrically, persistently mapped ----
    const size_t bytes = verts_.size() * sizeof(Vertex);
    if (bytes > vtxCapacity_) {
        WaitIdle();
        if (vtxMap_) { fnUnmap_(dev_, vtxMem_); vtxMap_ = nullptr; }
        if (vtxBuf_) { fnDestroyBuffer_(dev_, vtxBuf_, nullptr); vtxBuf_ = 0; }
        if (vtxMem_) { fnFreeMem_(dev_, vtxMem_, nullptr); vtxMem_ = 0; }
        size_t cap = std::max<size_t>(bytes, std::max<size_t>(vtxCapacity_ * 2, 64 * 1024));
        std::string err;
        if (!MakeBuffer(cap, BUFFER_VERTEX, MEM_HOST_VISIBLE | MEM_HOST_COHERENT, vtxBuf_, vtxMem_, &vtxMap_, err)) { std::fprintf(stderr, "[vulkan] %s\n", err.c_str()); ok_ = false; return; }
        vtxCapacity_ = cap;
    }
    if (bytes) std::memcpy(vtxMap_, verts_.data(), bytes);

    VkCommandBufferAllocateInfo ai = {ST_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, pool_, 0, 1};
    VkCommandBuffer cb = nullptr; if (fnAllocCmd_(dev_, &ai, &cb) != VK_SUCCESS) { ok_ = false; return; }
    VkCommandBufferBeginInfo bi = {ST_COMMAND_BUFFER_BEGIN_INFO, nullptr, CMD_ONE_TIME_SUBMIT, nullptr};
    fnBegin_(cb, &bi);
    VkClearValue clears[2]; clears[0].color[0] = clear_[0]; clears[0].color[1] = clear_[1]; clears[0].color[2] = clear_[2]; clears[0].color[3] = 1.0f; clears[1].ds.depth = 1.0f; clears[1].ds.stencil = 0;
    VkRenderPassBeginInfo rb = {ST_RENDER_PASS_BEGIN_INFO, nullptr, renderPass_, fb_, {{0, 0}, {(uint32_t)w_, (uint32_t)h_}}, 2, clears};
    fnCmdBeginRP_(cb, &rb, 0);
    fnCmdBindPipe_(cb, 0, pipeline_);
    VkViewport vp = {0, 0, (float)w_, (float)h_, 0.0f, 1.0f}; VkRect2D sc = {{0, 0}, {(uint32_t)w_, (uint32_t)h_}};
    fnCmdSetViewport_(cb, 0, 1, &vp); fnCmdSetScissor_(cb, 0, 1, &sc);
    if (vtxBuf_ && !verts_.empty()) {
        VkDeviceSize off = 0; fnCmdBindVB_(cb, 0, 1, &vtxBuf_, &off);
        for (const Draw& d : draws_) {
            if (d.count == 0) continue;
            const TextureId t = d.tex ? d.tex : whiteTex_;
            if (t > tex_.size() || !tex_[t - 1].set) continue;
            float pc[4] = {(float)w_, (float)h_, d.nearP, d.farP};
            fnCmdPush_(cb, pipeLayout_, STAGE_VERTEX, 0, 16, pc);
            fnCmdBindSets_(cb, 0, pipeLayout_, 0, 1, &tex_[t - 1].set, 0, nullptr);
            fnCmdDraw_(cb, (uint32_t)d.count, 1, (uint32_t)d.first, 0);
        }
    }
    fnCmdEndRP_(cb);          // the render pass leaves the colour image in TRANSFER_SRC
    VkBufferImageCopy cp = {0, 0, 0, {ASPECT_COLOR, 0, 0, 1}, {0, 0, 0}, {(uint32_t)w_, (uint32_t)h_, 1}};
    fnCmdCopyImgToBuf_(cb, colorImg_, LAYOUT_TRANSFER_SRC, readBuf_, 1, &cp);
    fnEnd_(cb);
    VkSubmitInfo si = {ST_SUBMIT_INFO, nullptr, 0, nullptr, nullptr, 1, &cb, 0, nullptr};
    fnResetFences_(dev_, 1, &fence_);
    fnSubmit_(queue_, 1, &si, fence_);
    fnWaitFences_(dev_, 1, &fence_, 1, ~0ull);
    fnFreeCmd_(dev_, pool_, 1, &cb);
    std::memcpy(frame_.data(), readMap_, (size_t)w_ * h_ * 4);
}

inline void VulkanDrawBackend::Composite(const uint32_t* canvas, std::vector<uint32_t>& px) {
    const size_t n = (size_t)w_ * h_;
    for (size_t i = 0; i < n; ++i) {
        const uint32_t s = canvas[i]; const uint32_t a = s >> 24;
        if (a == 0) continue;
        if (a == 255) { px[i] = s | 0xFF000000u; continue; }
        const uint32_t d = px[i], ia = 255 - a;
        const uint32_t r = (((s >> 16) & 255) * a + ((d >> 16) & 255) * ia + 127) / 255;
        const uint32_t g = (((s >> 8) & 255) * a + ((d >> 8) & 255) * ia + 127) / 255;
        const uint32_t b = ((s & 255) * a + (d & 255) * ia + 127) / 255;
        px[i] = 0xFF000000u | (r << 16) | (g << 8) | b;
    }
}

} // namespace DrawBackendNS
