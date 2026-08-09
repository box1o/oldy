#pragma once

#include <woki/core.hpp>

namespace woki::rhi {

enum class AdapterType : u32 {
    DiscreteGPU,
    IntegratedGPU,
    CPU,
    Unknown,
};

enum class AddressMode : u32 {
    Undefined,
    ClampToEdge,
    Repeat,
    MirrorRepeat,
};

enum class AlphaMode : u32 {
    Opaque,
    Premultiplied,
    Unpremultiplied,
};

enum class BackendType : u32 {
    Undefined,
    Null,
    WebGPU,
    D3D11,
    D3D12,
    Metal,
    Vulkan,
    OpenGL,
    OpenGLES,
};

enum class BlendFactor : u32 {
    Undefined,
    Zero,
    One,
    Src,
    OneMinusSrc,
    SrcAlpha,
    OneMinusSrcAlpha,
    Dst,
    OneMinusDst,
    DstAlpha,
    OneMinusDstAlpha,
    SrcAlphaSaturated,
    Constant,
    OneMinusConstant,
    Src1,
    OneMinusSrc1,
    Src1Alpha,
    OneMinusSrc1Alpha,
};

enum class BlendOperation : u32 {
    Undefined,
    Add,
    Subtract,
    ReverseSubtract,
    Min,
    Max,
};

enum class BufferBindingType : u32 {
    BindingNotUsed,
    Undefined,
    Uniform,
    Storage,
    ReadOnlyStorage,
};

enum class BufferMapState : u32 {
    Unmapped,
    Pending,
    Mapped,
};

enum class CallbackMode : u32 {
    WaitAnyOnly,
    AllowProcessEvents,
    AllowSpontaneous,
};

enum class ColorSpacePrimariesDawn : u32 {
    SRGB,
    Rec709,
    Rec601,
    Rec2020,
    DisplayP3,
};

enum class ColorSpaceTransferDawn : u32 {
    Identity,
    SRGB,
    DisplayP3,
    SMPTE_170M,
    HLG,
    PQ,
};

enum class ColorSpaceYCbCrMatrixDawn : u32 {
    Identity,
    Rec601,
    Rec709,
    Rec2020,
};

enum class ColorSpaceYCbCrRangeDawn : u32 {
    Identity,
    Narrow,
    Full,
};

enum class CompareFunction : u32 {
    Undefined,
    Never,
    Less,
    Equal,
    LessEqual,
    Greater,
    NotEqual,
    GreaterEqual,
    Always,
};

enum class CompilationInfoRequestStatus : u32 {
    Success,
    CallbackCancelled,
};

enum class CompilationMessageType : u32 {
    Error,
    Warning,
    Info,
};

enum class ComponentSwizzle : u32 {
    Undefined,
    Zero,
    One,
    R,
    G,
    B,
    A,
};

enum class CompositeAlphaMode : u32 {
    Auto,
    Opaque,
    Premultiplied,
    Unpremultiplied,
    Inherit,
};

enum class CreatePipelineAsyncStatus : u32 {
    Success,
    CallbackCancelled,
    ValidationError,
    InternalError,
};

enum class CullMode : u32 {
    Undefined,
    None,
    Front,
    Back,
};

enum class DeviceLostReason : u32 {
    Unknown,
    Destroyed,
    CallbackCancelled,
    FailedCreation,
};

enum class ErrorFilter : u32 {
    Validation,
    OutOfMemory,
    Internal,
};

enum class ErrorType : u32 {
    NoError,
    Validation,
    OutOfMemory,
    Internal,
    Unknown,
};

enum class ExternalTextureRotation : u32 {
    Rotate0Degrees,
    Rotate90Degrees,
    Rotate180Degrees,
    Rotate270Degrees,
};

enum class FeatureLevel : u32 {
    Undefined,
    Compatibility,
    Core,
};

enum class FeatureName : u32 {
    CoreFeaturesAndLimits,
    DepthClipControl,
    Depth32FloatStencil8,
    TextureCompressionBC,
    TextureCompressionBCSliced3D,
    TextureCompressionETC2,
    TextureCompressionASTC,
    TextureCompressionASTCSliced3D,
    TimestampQuery,
    IndirectFirstInstance,
    ShaderF16,
    RG11B10UfloatRenderable,
    BGRA8UnormStorage,
    Float32Filterable,
    Float32Blendable,
    ClipDistances,
    DualSourceBlending,
    Subgroups,
    TextureFormatsTier1,
    TextureFormatsTier2,
    PrimitiveIndex,
    TextureComponentSwizzle,
    DawnInternalUsages,
    DawnMultiPlanarFormats,
    DawnNative,
    ChromiumExperimentalTimestampQueryInsidePasses,
    ImplicitDeviceSynchronization,
    TransientAttachments,
    MSAARenderToSingleSampled,
    D3D11MultithreadProtected,
    ANGLETextureSharing,
    PixelLocalStorageCoherent,
    PixelLocalStorageNonCoherent,
    Unorm16TextureFormats,
    MultiPlanarFormatExtendedUsages,
    MultiPlanarFormatP010,
    HostMappedPointer,
    MultiPlanarRenderTargets,
    MultiPlanarFormatNv12a,
    FramebufferFetch,
    BufferMapExtendedUsages,
    AdapterPropertiesMemoryHeaps,
    AdapterPropertiesD3D,
    AdapterPropertiesVk,
    DawnFormatCapabilities,
    DawnDrmFormatCapabilities,
    MultiPlanarFormatNv16,
    MultiPlanarFormatNv24,
    MultiPlanarFormatP210,
    MultiPlanarFormatP410,
    SharedTextureMemoryVkDedicatedAllocation,
    SharedTextureMemoryAHardwareBuffer,
    SharedTextureMemoryDmaBuf,
    SharedTextureMemoryOpaqueFD,
    SharedTextureMemoryZirconHandle,
    SharedTextureMemoryDXGISharedHandle,
    SharedTextureMemoryD3D11Texture2D,
    SharedTextureMemoryIOSurface,
    SharedTextureMemoryEGLImage,
    SharedFenceVkSemaphoreOpaqueFD,
    SharedFenceSyncFD,
    SharedFenceVkSemaphoreZirconHandle,
    SharedFenceDXGISharedHandle,
    SharedFenceMTLSharedEvent,
    SharedBufferMemoryD3D12Resource,
    StaticSamplers,
    YCbCrVulkanSamplers,
    ShaderModuleCompilationOptions,
    DawnLoadResolveTexture,
    DawnPartialLoadResolveTexture,
    MultiDrawIndirect,
    DawnTexelCopyBufferRowAlignment,
    FlexibleTextureViews,
    ChromiumExperimentalSubgroupMatrix,
    SharedFenceEGLSync,
    DawnDeviceAllocatorControl,
    AdapterPropertiesWGPU,
    SharedBufferMemoryD3D12SharedMemoryFileMappingHandle,
    SharedTextureMemoryD3D12Resource,
    ChromiumExperimentalSamplingResourceTable,
    SubgroupSizeControl,
    AtomicVec2uMinMax,
    Unorm16FormatsForExternalTexture,
    OpaqueYCbCrAndroidForExternalTexture,
    Unorm16Filterable,
    RenderPassRenderArea,
    AdapterPropertiesDrm,
};

enum class FilterMode : u32 {
    Undefined,
    Nearest,
    Linear,
};

enum class FrontFace : u32 {
    Undefined,
    CCW,
    CW,
};

enum class IndexFormat : u32 {
    Undefined,
    Uint16,
    Uint32,
};

enum class InstanceFeatureName : u32 {
    TimedWaitAny,
    ShaderSourceSPIRV,
    MultipleDevicesPerAdapter,
};

enum class LoadOp : u32 {
    Undefined,
    Load,
    Clear,
    ExpandResolveTexture,
};

enum class LoggingType : u32 {
    Verbose,
    Info,
    Warning,
    Error,
};

enum class MapAsyncStatus : u32 {
    Success,
    CallbackCancelled,
    Error,
    Aborted,
};

enum class MipmapFilterMode : u32 {
    Undefined,
    Nearest,
    Linear,
};

enum class PopErrorScopeStatus : u32 {
    Success,
    CallbackCancelled,
    Error,
};

enum class PowerPreference : u32 {
    Undefined,
    LowPower,
    HighPerformance,
};

enum class PredefinedColorSpace : u32 {
    SRGB,
    DisplayP3,
    SRGBLinear,
    DisplayP3Linear,
    Rec2020Linear,
};

enum class PresentMode : u32 {
    Undefined,
    Fifo,
    FifoRelaxed,
    Immediate,
    Mailbox,
};

enum class PrimitiveTopology : u32 {
    Undefined,
    PointList,
    LineList,
    LineStrip,
    TriangleList,
    TriangleStrip,
};

enum class QueryType : u32 {
    Occlusion,
    Timestamp,
};

enum class QueueWorkDoneStatus : u32 {
    Success,
    CallbackCancelled,
    Error,
};

enum class RequestAdapterStatus : u32 {
    Success,
    CallbackCancelled,
    Unavailable,
    Error,
};

enum class RequestDeviceStatus : u32 {
    Success,
    CallbackCancelled,
    Error,
};

enum class SamplerBindingType : u32 {
    BindingNotUsed,
    Undefined,
    Filtering,
    NonFiltering,
    Comparison,
};

enum class SharedFenceType : u32 {
    VkSemaphoreOpaqueFD,
    SyncFD,
    VkSemaphoreZirconHandle,
    DXGISharedHandle,
    MTLSharedEvent,
    EGLSync,
};

enum class Status : u32 {
    Success,
    Error,
};

enum class StencilOperation : u32 {
    Undefined,
    Keep,
    Zero,
    Replace,
    Invert,
    IncrementClamp,
    DecrementClamp,
    IncrementWrap,
    DecrementWrap,
};

enum class StorageTextureAccess : u32 {
    BindingNotUsed,
    Undefined,
    WriteOnly,
    ReadOnly,
    ReadWrite,
};

enum class StoreOp : u32 {
    Undefined,
    Store,
    Discard,
};

enum class SType : u32 {
    ShaderSourceSPIRV,
    ShaderSourceWGSL,
    RenderPassMaxDrawCount,
    SurfaceSourceMetalLayer,
    SurfaceSourceWindowsHWND,
    SurfaceSourceXlibWindow,
    SurfaceSourceWaylandSurface,
    SurfaceSourceAndroidNativeWindow,
    SurfaceSourceXCBWindow,
    SurfaceColorManagement,
    RequestAdapterWebXROptions,
    TextureComponentSwizzleDescriptor,
    ExternalTextureBindingLayout,
    ExternalTextureBindingEntry,
    CompatibilityModeLimits,
    TextureBindingViewDimension,
    EmscriptenSurfaceSourceCanvasHTMLSelector,
    SurfaceDescriptorFromWindowsCoreWindow,
    SurfaceDescriptorFromWindowsUWPSwapChainPanel,
    DawnTextureInternalUsageDescriptor,
    DawnEncoderInternalUsageDescriptor,
    DawnInstanceDescriptor,
    DawnCacheDeviceDescriptor,
    DawnAdapterPropertiesPowerPreference,
    DawnBufferDescriptorErrorInfoFromWireClient,
    DawnTogglesDescriptor,
    DawnShaderModuleSPIRVOptionsDescriptor,
    RequestAdapterOptionsLUID,
    RequestAdapterOptionsGetGLProc,
    RequestAdapterOptionsD3D11Device,
    DawnRenderPassSampleCount,
    RenderPassPixelLocalStorage,
    PipelineLayoutPixelLocalStorage,
    BufferHostMappedPointer,
    AdapterPropertiesMemoryHeaps,
    AdapterPropertiesD3D,
    AdapterPropertiesVk,
    DawnWireWGSLControl,
    DawnWGSLBlocklist,
    DawnDrmFormatCapabilities,
    ShaderModuleCompilationOptions,
    ColorTargetStateExpandResolveTextureDawn,
    RenderPassRenderAreaRect,
    SharedTextureMemoryVkDedicatedAllocationDescriptor,
    SharedTextureMemoryAHardwareBufferDescriptor,
    SharedTextureMemoryDmaBufDescriptor,
    SharedTextureMemoryOpaqueFDDescriptor,
    SharedTextureMemoryZirconHandleDescriptor,
    SharedTextureMemoryDXGISharedHandleDescriptor,
    SharedTextureMemoryD3D11Texture2DDescriptor,
    SharedTextureMemoryIOSurfaceDescriptor,
    SharedTextureMemoryEGLImageDescriptor,
    SharedTextureMemoryInitializedBeginState,
    SharedTextureMemoryInitializedEndState,
    SharedTextureMemoryVkImageLayoutBeginState,
    SharedTextureMemoryVkImageLayoutEndState,
    SharedTextureMemoryD3DSwapchainBeginState,
    SharedFenceVkSemaphoreOpaqueFDDescriptor,
    SharedFenceVkSemaphoreOpaqueFDExportInfo,
    SharedFenceSyncFDDescriptor,
    SharedFenceSyncFDExportInfo,
    SharedFenceVkSemaphoreZirconHandleDescriptor,
    SharedFenceVkSemaphoreZirconHandleExportInfo,
    SharedFenceDXGISharedHandleDescriptor,
    SharedFenceDXGISharedHandleExportInfo,
    SharedFenceMTLSharedEventDescriptor,
    SharedFenceMTLSharedEventExportInfo,
    SharedBufferMemoryD3D12ResourceDescriptor,
    StaticSamplerBindingLayout,
    YCbCrVkDescriptor,
    SharedTextureMemoryAHardwareBufferProperties,
    AHardwareBufferProperties,
    DawnTexelCopyBufferRowAlignmentLimits,
    AdapterPropertiesSubgroupMatrixConfigs,
    SharedFenceEGLSyncDescriptor,
    SharedFenceEGLSyncExportInfo,
    DawnInjectedInvalidSType,
    DawnCompilationMessageUtf16,
    DawnFakeBufferOOMForTesting,
    SurfaceDescriptorFromWindowsWinUISwapChainPanel,
    DawnDeviceAllocatorControl,
    DawnHostMappedPointerLimits,
    RenderPassDescriptorResolveRect,
    RequestAdapterWebGPUBackendOptions,
    DawnFakeDeviceInitializeErrorForTesting,
    SharedTextureMemoryD3D11BeginState,
    DawnConsumeAdapterDescriptor,
    TexelBufferBindingEntry,
    TexelBufferBindingLayout,
    SharedTextureMemoryMetalEndAccessState,
    AdapterPropertiesWGPU,
    SharedBufferMemoryD3D12SharedMemoryFileMappingHandleDescriptor,
    SharedTextureMemoryD3D12ResourceDescriptor,
    RequestAdapterOptionsAngleVirtualizationGroup,
    PipelineLayoutResourceTable,
    AdapterPropertiesDrm,
};

enum class SubgroupMatrixComponentType : u32 {
    F32,
    F16,
    U32,
    I32,
    U8,
    I8,
};

enum class SurfaceGetCurrentTextureStatus : u32 {
    SuccessOptimal,
    SuccessSuboptimal,
    Timeout,
    Outdated,
    Lost,
    Error,
};

enum class TexelBufferAccess : u32 {
    Undefined,
    ReadOnly,
    ReadWrite,
};

enum class TextureAspect : u32 {
    Undefined,
    All,
    StencilOnly,
    DepthOnly,
    Plane0Only,
    Plane1Only,
    Plane2Only,
};

enum class TextureDimension : u32 {
    Undefined,
    e1D,
    e2D,
    e3D,
};

enum class TextureFormat : u32 {
    Undefined,
    R8Unorm,
    R8Snorm,
    R8Uint,
    R8Sint,
    R16Unorm,
    R16Snorm,
    R16Uint,
    R16Sint,
    R16Float,
    RG8Unorm,
    RG8Snorm,
    RG8Uint,
    RG8Sint,
    R32Float,
    R32Uint,
    R32Sint,
    RG16Unorm,
    RG16Snorm,
    RG16Uint,
    RG16Sint,
    RG16Float,
    RGBA8Unorm,
    RGBA8UnormSrgb,
    RGBA8Snorm,
    RGBA8Uint,
    RGBA8Sint,
    BGRA8Unorm,
    BGRA8UnormSrgb,
    RGB10A2Uint,
    RGB10A2Unorm,
    RG11B10Ufloat,
    RGB9E5Ufloat,
    RG32Float,
    RG32Uint,
    RG32Sint,
    RGBA16Unorm,
    RGBA16Snorm,
    RGBA16Uint,
    RGBA16Sint,
    RGBA16Float,
    RGBA32Float,
    RGBA32Uint,
    RGBA32Sint,
    Stencil8,
    Depth16Unorm,
    Depth24Plus,
    Depth24PlusStencil8,
    Depth32Float,
    Depth32FloatStencil8,
    BC1RGBAUnorm,
    BC1RGBAUnormSrgb,
    BC2RGBAUnorm,
    BC2RGBAUnormSrgb,
    BC3RGBAUnorm,
    BC3RGBAUnormSrgb,
    BC4RUnorm,
    BC4RSnorm,
    BC5RGUnorm,
    BC5RGSnorm,
    BC6HRGBUfloat,
    BC6HRGBFloat,
    BC7RGBAUnorm,
    BC7RGBAUnormSrgb,
    ETC2RGB8Unorm,
    ETC2RGB8UnormSrgb,
    ETC2RGB8A1Unorm,
    ETC2RGB8A1UnormSrgb,
    ETC2RGBA8Unorm,
    ETC2RGBA8UnormSrgb,
    EACR11Unorm,
    EACR11Snorm,
    EACRG11Unorm,
    EACRG11Snorm,
    ASTC4x4Unorm,
    ASTC4x4UnormSrgb,
    ASTC5x4Unorm,
    ASTC5x4UnormSrgb,
    ASTC5x5Unorm,
    ASTC5x5UnormSrgb,
    ASTC6x5Unorm,
    ASTC6x5UnormSrgb,
    ASTC6x6Unorm,
    ASTC6x6UnormSrgb,
    ASTC8x5Unorm,
    ASTC8x5UnormSrgb,
    ASTC8x6Unorm,
    ASTC8x6UnormSrgb,
    ASTC8x8Unorm,
    ASTC8x8UnormSrgb,
    ASTC10x5Unorm,
    ASTC10x5UnormSrgb,
    ASTC10x6Unorm,
    ASTC10x6UnormSrgb,
    ASTC10x8Unorm,
    ASTC10x8UnormSrgb,
    ASTC10x10Unorm,
    ASTC10x10UnormSrgb,
    ASTC12x10Unorm,
    ASTC12x10UnormSrgb,
    ASTC12x12Unorm,
    ASTC12x12UnormSrgb,
    R8BG8Biplanar420Unorm,
    R10X6BG10X6Biplanar420Unorm,
    R8BG8A8Triplanar420Unorm,
    R8BG8Biplanar422Unorm,
    R8BG8Biplanar444Unorm,
    R10X6BG10X6Biplanar422Unorm,
    R10X6BG10X6Biplanar444Unorm,
    OpaqueYCbCrAndroid,
};

enum class TextureSampleType : u32 {
    BindingNotUsed,
    Undefined,
    Float,
    UnfilterableFloat,
    Depth,
    Sint,
    Uint,
};

enum class TextureViewDimension : u32 {
    Undefined,
    e1D,
    e2D,
    e2DArray,
    Cube,
    CubeArray,
    e3D,
};

enum class ToneMappingMode : u32 {
    Standard,
    Extended,
};

enum class VertexFormat : u32 {
    Uint8,
    Uint8x2,
    Uint8x4,
    Sint8,
    Sint8x2,
    Sint8x4,
    Unorm8,
    Unorm8x2,
    Unorm8x4,
    Snorm8,
    Snorm8x2,
    Snorm8x4,
    Uint16,
    Uint16x2,
    Uint16x4,
    Sint16,
    Sint16x2,
    Sint16x4,
    Unorm16,
    Unorm16x2,
    Unorm16x4,
    Snorm16,
    Snorm16x2,
    Snorm16x4,
    Float16,
    Float16x2,
    Float16x4,
    Float32,
    Float32x2,
    Float32x3,
    Float32x4,
    Uint32,
    Uint32x2,
    Uint32x3,
    Uint32x4,
    Sint32,
    Sint32x2,
    Sint32x3,
    Sint32x4,
    Unorm10_10_10_2,
    Unorm8x4BGRA,
};

enum class VertexStepMode : u32 {
    Undefined,
    Vertex,
    Instance,
};

enum class WaitStatus : u32 {
    Success,
    TimedOut,
    Error,
};

enum class WGSLLanguageFeatureName : u32 {
    ReadonlyAndReadwriteStorageTextures,
    Packed4x8IntegerDotProduct,
    UnrestrictedPointerParameters,
    PointerCompositeAccess,
    UniformBufferStandardLayout,
    SubgroupId,
    TextureAndSamplerLet,
    SubgroupUniformity,
    TextureFormatsTier1,
    LinearIndexing,
    ImmediateAddressSpace,
    ChromiumTestingUnimplemented,
    ChromiumTestingUnsafeExperimental,
    ChromiumTestingExperimental,
    ChromiumTestingShippedWithKillswitch,
    ChromiumTestingShipped,
    SizedBindingArray,
    TexelBuffers,
    ChromiumPrint,
    FragmentDepth,
    BufferView,
    SwizzleAssignment,
};

enum class BufferUsage : u64 {
    None = 0,
    MapRead = 1ull << 0,
    MapWrite = 1ull << 1,
    CopySrc = 1ull << 2,
    CopyDst = 1ull << 3,
    Index = 1ull << 4,
    Vertex = 1ull << 5,
    Uniform = 1ull << 6,
    Storage = 1ull << 7,
    Indirect = 1ull << 8,
    QueryResolve = 1ull << 9,
    TexelBuffer = 1ull << 10,
};

enum class ColorWriteMask : u64 {
    None,
    Red,
    Green,
    Blue,
    Alpha,
    All,
};

enum class HeapProperty : u64 {
    None,
    DeviceLocal,
    HostVisible,
    HostCoherent,
    HostUncached,
    HostCached,
};

enum class MapMode : u64 {
    None,
    Read,
    Write,
};

enum class ShaderStage : u64 {
    None = 0,
    Vertex = 1ull << 0,
    Fragment = 1ull << 1,
    Compute = 1ull << 2,
};

enum class TextureUsage : u64 {
    None = 0,
    CopySrc = 1ull << 0,
    CopyDst = 1ull << 1,
    TextureBinding = 1ull << 2,
    StorageBinding = 1ull << 3,
    RenderAttachment = 1ull << 4,
    TransientAttachment = 1ull << 5,
    StorageAttachment = 1ull << 6,
};

} // namespace woki::rhi
