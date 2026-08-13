#pragma once

#include <span>
#include <string>
#include <vector>
#include <optional>

#include "types.hpp"
#include "forward.hpp"

namespace woki::rhi {

struct InstanceDesc final {
    void* next_in_chain{nullptr};
    std::vector<InstanceFeatureName> required_features{};
    std::optional<InstanceLimits> required_limits{};
    bool enable_validation{false};
    std::string label{"Instance"};
};

struct RequestAdapterDesc final {
    Surface* compatible_surface{nullptr};
    FeatureLevel feature_level{FeatureLevel::Undefined};
    BackendType backend_type{BackendType::Undefined};
    PowerPreference power_preference{PowerPreference::HighPerformance};
    bool force_fallback_adapter{false};
};

struct SurfaceDesc final {
    std::string label{"Surface"};
};

// `next_in_chain` is reserved for backend-owned platform surface extensions.
struct SurfaceDescriptor final {
    void* next_in_chain{nullptr};
    std::string label{};
};

struct SurfaceCapabilities final {
    TextureUsage usages{TextureUsage::None};
    std::vector<TextureFormat> formats{};
    std::vector<PresentMode> present_modes{};
    std::vector<CompositeAlphaMode> alpha_modes{};
};

struct SurfaceConfiguration final {
    Device* device{nullptr};
    TextureFormat format{TextureFormat::BGRA8Unorm};
    TextureUsage usage{TextureUsage::RenderAttachment};
    u32 width{0};
    u32 height{0};
    std::vector<TextureFormat> view_formats{};
    CompositeAlphaMode alpha_mode{CompositeAlphaMode::Auto};
    PresentMode present_mode{PresentMode::Fifo};
};

struct SurfaceTexture final {
    SurfaceGetCurrentTextureStatus status{SurfaceGetCurrentTextureStatus::Error};
    NativeHandles handles{};
    bool suboptimal{false};
};

struct AdapterInfo final {
    std::string vendor{};
    std::string architecture{};
    std::string device{};
    std::string description{};
    BackendType backend_type{BackendType::Undefined};
    AdapterType adapter_type{AdapterType::Unknown};
    FeatureLevel feature_level{FeatureLevel::Undefined};
    u32 vendor_id{0};
    u32 device_id{0};
    u32 subgroup_min_size{0};
    u32 subgroup_max_size{0};
};

struct SupportedFeatures final {
    std::vector<FeatureName> values{};

    [[nodiscard]] bool Has(FeatureName value) const noexcept {
        return std::ranges::find(values, value) != values.end();
    }
};

struct Limits final {
    void* next_in_chain{nullptr};
    u32 max_texture_dimension_1d{kLimitU32Undefined};
    u32 max_texture_dimension_2d{kLimitU32Undefined};
    u32 max_texture_dimension_3d{kLimitU32Undefined};
    u32 max_texture_array_layers{kLimitU32Undefined};
    u32 max_bind_groups{kLimitU32Undefined};
    u32 max_bind_groups_plus_vertex_buffers{kLimitU32Undefined};
    u32 max_bindings_per_bind_group{kLimitU32Undefined};
    u32 max_dynamic_uniform_buffers_per_pipeline_layout{kLimitU32Undefined};
    u32 max_dynamic_storage_buffers_per_pipeline_layout{kLimitU32Undefined};
    u32 max_sampled_textures_per_shader_stage{kLimitU32Undefined};
    u32 max_samplers_per_shader_stage{kLimitU32Undefined};
    u32 max_storage_buffers_per_shader_stage{kLimitU32Undefined};
    u32 max_storage_textures_per_shader_stage{kLimitU32Undefined};
    u32 max_uniform_buffers_per_shader_stage{kLimitU32Undefined};
    u64 max_uniform_buffer_binding_size{kLimitU64Undefined};
    u64 max_storage_buffer_binding_size{kLimitU64Undefined};
    u32 min_uniform_buffer_offset_alignment{kLimitU32Undefined};
    u32 min_storage_buffer_offset_alignment{kLimitU32Undefined};
    u32 max_vertex_buffers{kLimitU32Undefined};
    u64 max_buffer_size{kLimitU64Undefined};
    u32 max_vertex_attributes{kLimitU32Undefined};
    u32 max_vertex_buffer_array_stride{kLimitU32Undefined};
    u32 max_inter_stage_shader_variables{kLimitU32Undefined};
    u32 max_color_attachments{kLimitU32Undefined};
    u32 max_color_attachment_bytes_per_sample{kLimitU32Undefined};
    u32 max_compute_workgroup_storage_size{kLimitU32Undefined};
    u32 max_compute_invocations_per_workgroup{kLimitU32Undefined};
    u32 max_compute_workgroup_size_x{kLimitU32Undefined};
    u32 max_compute_workgroup_size_y{kLimitU32Undefined};
    u32 max_compute_workgroup_size_z{kLimitU32Undefined};
    u32 max_compute_workgroups_per_dimension{kLimitU32Undefined};
    u32 max_immediate_size{kLimitU32Undefined};
};

struct DawnFormatCapabilities final {
    void* next_in_chain{nullptr};
};

struct QueueDesc final {
    std::string label{"Queue"};
};

struct DeviceDesc final {
    std::vector<FeatureName> required_features{};
    std::optional<Limits> required_limits{};
    QueueDesc default_queue{};
    DeviceLostCallback device_lost_callback{};
    UncapturedErrorCallback uncaptured_error_callback{};
    std::string label{"Device"};
};

struct Origin2D final {
    u32 x{0};
    u32 y{0};
};

struct Origin3D final {
    u32 x{0};
    u32 y{0};
    u32 z{0};
};

struct Extent3D final {
    u32 width{0};
    u32 height{0};
    u32 depth_or_array_layers{0};
};

struct Extent2D final {
    u32 width{0};
    u32 height{0};
};

struct TexelCopyBufferLayout final {
    u64 offset{0};
    u32 bytes_per_row{kCopyStrideUndefined};
    u32 rows_per_image{kCopyStrideUndefined};
};

struct TexelCopyBufferInfo final {
    TexelCopyBufferLayout layout{};
    Buffer* buffer{nullptr};
};

struct Color final {
    f64 r{0.0};
    f64 g{0.0};
    f64 b{0.0};
    f64 a{0.0};
};

struct CommandBufferDesc final {
    void* next_in_chain{nullptr};
    std::string label{"CommandBuffer"};
};

struct PassTimestampWritesDesc final {
    void* next_in_chain{nullptr};
    QuerySet* query_set{nullptr};
    u32 beginning_of_pass_write_index{kQuerySetIndexUndefined};
    u32 end_of_pass_write_index{kQuerySetIndexUndefined};
};

struct ComputePassDesc final {
    void* next_in_chain{nullptr};
    std::string label{"ComputePass"};
    const PassTimestampWritesDesc* timestamp_writes{nullptr};
};

struct RenderPassDesc final {
    void* next_in_chain{nullptr};
    std::string label{"RenderPass"};
    void* timestamp_writes{nullptr};
    void* occlusion_query_set{nullptr};
    u32 color_attachment_count{0};
    void* color_attachments{nullptr};
    void* depth_stencil_attachment{nullptr};
};

struct RenderPassColorAttachmentDesc final {
    void* next_in_chain{nullptr};
    TextureView* view{nullptr};
    TextureView* resolve_target{nullptr};
    u32 depth_slice{kDepthSliceUndefined};
    LoadOp load_op{LoadOp::Clear};
    StoreOp store_op{StoreOp::Store};
    Color clear_value{0.12, 0.12, 0.18, 1.0};
};

struct RenderPassDepthStencilAttachmentDesc final {
    void* next_in_chain{nullptr};
    TextureView* view{nullptr};
    LoadOp depth_load_op{LoadOp::Clear};
    StoreOp depth_store_op{StoreOp::Store};
    f32 depth_clear_value{1.0f};
    bool depth_read_only{false};
    LoadOp stencil_load_op{LoadOp::Undefined};
    StoreOp stencil_store_op{StoreOp::Undefined};
    u32 stencil_clear_value{0};
    bool stencil_read_only{true};
};

struct RenderPassDescTyped final {
    void* next_in_chain{nullptr};
    std::string label{"RenderPass"};
    const PassTimestampWritesDesc* timestamp_writes{nullptr};
    QuerySet* occlusion_query_set{nullptr};
    std::span<const RenderPassColorAttachmentDesc> color_attachments{};
    const RenderPassDepthStencilAttachmentDesc* depth_stencil_attachment{nullptr};
};

struct TexelCopyTextureInfo final {
    Texture* texture{nullptr};
    u32 mip_level{0};
    Origin3D origin{};
    TextureAspect aspect{TextureAspect::Undefined};
};

struct CopyTextureForBrowserOptions final {
    void* next_in_chain{nullptr};
    bool flip_y{false};
    bool needs_color_space_conversion{false};
    AlphaMode src_alpha_mode{AlphaMode::Unpremultiplied};
    const f32* src_transfer_function_parameters{nullptr};
    const f32* conversion_matrix{nullptr};
    const f32* dst_transfer_function_parameters{nullptr};
    AlphaMode dst_alpha_mode{AlphaMode::Unpremultiplied};
    bool internal_usage{false};
};

struct ImageCopyExternalTexture final {
    void* external_texture{nullptr};
    Origin3D origin{};
    Extent2D natural_size{};
};

struct CompilationMessage final {
    CompilationMessageType type{CompilationMessageType::Error};
    std::string message{};
    u64 line_num{0};
    u64 line_pos{0};
    u64 offset{0};
    u64 length{0};
};

struct CompilationInfo final {
    std::vector<CompilationMessage> messages{};
};

struct StencilFaceStateDesc final {
    CompareFunction compare{CompareFunction::Undefined};
    StencilOperation fail_op{StencilOperation::Undefined};
    StencilOperation depth_fail_op{StencilOperation::Undefined};
    StencilOperation pass_op{StencilOperation::Undefined};
};

struct BlendComponentDesc final {
    BlendOperation operation{BlendOperation::Add};
    BlendFactor src_factor{BlendFactor::One};
    BlendFactor dst_factor{BlendFactor::Zero};
};

struct BlendStateDesc final {
    BlendComponentDesc color{};
    BlendComponentDesc alpha{};
};

struct BufferBindingLayoutDesc final {
    void* next_in_chain{nullptr};
    BufferBindingType type{BufferBindingType::BindingNotUsed};
    bool has_dynamic_offset{false};
    u64 min_binding_size{0};
};

struct SamplerBindingLayoutDesc final {
    void* next_in_chain{nullptr};
    SamplerBindingType type{SamplerBindingType::BindingNotUsed};
};

struct TextureBindingLayoutDesc final {
    void* next_in_chain{nullptr};
    TextureSampleType sample_type{TextureSampleType::BindingNotUsed};
    TextureViewDimension view_dimension{TextureViewDimension::Undefined};
    bool multisampled{false};
};

struct StorageTextureBindingLayoutDesc final {
    void* next_in_chain{nullptr};
    StorageTextureAccess access{StorageTextureAccess::BindingNotUsed};
    TextureFormat format{TextureFormat::Undefined};
    TextureViewDimension view_dimension{TextureViewDimension::Undefined};
};

struct BindGroupLayoutEntryDesc final {
    void* next_in_chain{nullptr};
    u32 binding{0};
    u32 visibility{0};
    u32 binding_array_size{0};
    BufferBindingLayoutDesc buffer{};
    SamplerBindingLayoutDesc sampler{};
    TextureBindingLayoutDesc texture{};
    StorageTextureBindingLayoutDesc storage_texture{};
};

struct BindGroupEntryDesc final {
    void* next_in_chain{nullptr};
    u32 binding{0};
    Buffer* buffer{nullptr};
    u64 offset{0};
    u64 size{kWholeSize};
    Sampler* sampler{nullptr};
    TextureView* texture_view{nullptr};
};

struct ConstantEntryDesc final {
    std::string key{};
    f64 value{0.0};
};

struct ComputeStateDesc final {
    void* next_in_chain{nullptr};
    ShaderModule* module{nullptr};
    std::string entry_point{"main"};
    std::span<const ConstantEntryDesc> constants{};
};

struct TextureDesc final {
    void* next_in_chain{nullptr};
    Extent3D size{};
    u32 mip_level_count{1};
    u32 sample_count{1};
    TextureDimension dimension{TextureDimension::e2D};
    TextureFormat format{TextureFormat::Undefined};
    TextureUsage usage{};
    std::vector<TextureFormat> view_formats{};
    std::string label{"Texture"};
};

struct BindGroupDesc final {
    void* next_in_chain{nullptr};
    BindGroupLayout* layout{nullptr};
    std::span<const BindGroupEntryDesc> entries{};
    std::string label{"BindGroup"};
};

struct BindGroupLayoutDesc final {
    void* next_in_chain{nullptr};
    std::span<const BindGroupLayoutEntryDesc> entries{};
    std::string label{"BindGroupLayout"};
};

struct BufferDesc final {
    void* next_in_chain{nullptr};
    u64 size{0};
    BufferUsage usage{};
    bool mapped_at_creation{false};
    std::string label{"Buffer"};
};

struct CommandEncoderDesc final {
    void* next_in_chain{nullptr};
    std::string label{"CommandEncoder"};
};

struct ComputePipelineDesc final {
    void* next_in_chain{nullptr};
    PipelineLayout* layout{nullptr};
    ComputeStateDesc compute{};
    std::string label{"ComputePipeline"};
};

struct ExternalTextureDesc final {
    void* next_in_chain{nullptr};
    TextureView* plane0{nullptr};
    TextureView* plane1{nullptr};
    Origin2D crop_origin{};
    Extent2D crop_size{};
    Extent2D apparent_size{};
    bool do_yuv_to_rgb_conversion_only{false};
    const f32* yuv_to_rgb_conversion_matrix{nullptr};
    const f32* src_transfer_function_parameters{nullptr};
    const f32* dst_transfer_function_parameters{nullptr};
    const f32* gamut_conversion_matrix{nullptr};
    bool mirrored{false};
    ExternalTextureRotation rotation{ExternalTextureRotation::Rotate0Degrees};
    std::string label{"ExternalTexture"};
};

struct PipelineLayoutDesc final {
    void* next_in_chain{nullptr};
    std::span<BindGroupLayout* const> bind_group_layouts{};
    u32 immediate_size{0};
    std::string label{"PipelineLayout"};
};

struct QuerySetDesc final {
    void* next_in_chain{nullptr};
    QueryType type{QueryType::Occlusion};
    u32 count{0};
    std::string label{"QuerySet"};
};

struct RenderBundleEncoderDesc final {
    void* next_in_chain{nullptr};
    std::vector<TextureFormat> color_formats{};
    TextureFormat depth_stencil_format{TextureFormat::Undefined};
    u32 sample_count{1};
    bool depth_read_only{false};
    bool stencil_read_only{false};
    std::string label{"RenderBundleEncoder"};
};

struct RenderPipelineDesc final {
    void* next_in_chain{nullptr};
    PipelineLayout* layout{nullptr};
    void* vertex{nullptr};
    void* primitive{nullptr};
    void* depth_stencil{nullptr};
    void* multisample{nullptr};
    void* fragment{nullptr};
    std::string label{"RenderPipeline"};
};

struct VertexAttributeDesc final {
    void* next_in_chain{nullptr};
    VertexFormat format{VertexFormat::Float32x3};
    u64 offset{0};
    u32 shader_location{0};
};

struct VertexBufferLayoutDesc final {
    void* next_in_chain{nullptr};
    VertexStepMode step_mode{VertexStepMode::Vertex};
    u64 array_stride{0};
    std::span<const VertexAttributeDesc> attributes{};
};

struct VertexStateDesc final {
    void* next_in_chain{nullptr};
    ShaderModule* module{nullptr};
    std::string entry_point{"main"};
    std::span<const ConstantEntryDesc> constants{};
    std::span<const VertexBufferLayoutDesc> buffers{};
};

struct ColorTargetStateDesc final {
    void* next_in_chain{nullptr};
    TextureFormat format{TextureFormat::BGRA8Unorm};
    const BlendStateDesc* blend{nullptr};
    ColorWriteMask write_mask{ColorWriteMask::All};
};

struct FragmentStateDesc final {
    void* next_in_chain{nullptr};
    ShaderModule* module{nullptr};
    std::string entry_point{"main"};
    std::span<const ConstantEntryDesc> constants{};
    std::span<const ColorTargetStateDesc> targets{};
};

struct PrimitiveStateDesc final {
    void* next_in_chain{nullptr};
    PrimitiveTopology topology{PrimitiveTopology::TriangleList};
    IndexFormat strip_index_format{IndexFormat::Undefined};
    FrontFace front_face{FrontFace::CCW};
    CullMode cull_mode{CullMode::None};
    bool unclipped_depth{false};
};

struct DepthStencilStateDesc final {
    void* next_in_chain{nullptr};
    TextureFormat format{TextureFormat::Depth24PlusStencil8};
    std::optional<bool> depth_write_enabled{true};
    CompareFunction depth_compare{CompareFunction::Less};
    StencilFaceStateDesc stencil_front{};
    StencilFaceStateDesc stencil_back{};
    u32 stencil_read_mask{0xFFFFFFFF};
    u32 stencil_write_mask{0xFFFFFFFF};
    i32 depth_bias{0};
    f32 depth_bias_slope_scale{0.0f};
    f32 depth_bias_clamp{0.0f};
};

struct MultisampleStateDesc final {
    u32 count{1};
    u32 mask{0xFFFFFFFF};
    bool alpha_to_coverage_enabled{false};
};

struct RenderPipelineDescTyped final {
    void* next_in_chain{nullptr};
    PipelineLayout* layout{nullptr};
    const VertexStateDesc* vertex{nullptr};
    const PrimitiveStateDesc* primitive{nullptr};
    const DepthStencilStateDesc* depth_stencil{nullptr};
    MultisampleStateDesc multisample{};
    const FragmentStateDesc* fragment{nullptr};
    std::string label{"RenderPipeline"};
};

struct ResourceTableDesc final {
    void* next_in_chain{nullptr};
    u32 size{0};
    std::string label{"ResourceTable"};
};

struct SamplerDesc final {
    void* next_in_chain{nullptr};
    AddressMode address_mode_u{AddressMode::ClampToEdge};
    AddressMode address_mode_v{AddressMode::ClampToEdge};
    AddressMode address_mode_w{AddressMode::ClampToEdge};
    FilterMode mag_filter{FilterMode::Nearest};
    FilterMode min_filter{FilterMode::Nearest};
    MipmapFilterMode mipmap_filter{MipmapFilterMode::Nearest};
    f32 lod_min_clamp{0.0f};
    f32 lod_max_clamp{32.0f};
    CompareFunction compare{CompareFunction::Undefined};
    u16 max_anisotropy{1};
    std::string label{"Sampler"};
};

struct ShaderModuleDesc final {
    void* next_in_chain{nullptr};
    std::string code{};
    std::string label{"ShaderModule"};
};

struct SharedBufferMemoryDesc final {
    void* next_in_chain{nullptr};
    std::string label{"SharedBufferMemory"};
};

struct SharedFenceDesc final {
    void* next_in_chain{nullptr};
    std::string label{"SharedFence"};
};

struct SharedTextureMemoryDesc final {
    void* next_in_chain{nullptr};
    std::string label{"SharedTextureMemory"};
};

struct TexelBufferViewDesc final {
    void* next_in_chain{nullptr};
    TextureFormat format{TextureFormat::Undefined};
    u64 offset{0};
    u64 size{kWholeSize};
    std::string label{"TexelBufferView"};
};

struct TextureViewDesc final {
    void* next_in_chain{nullptr};
    TextureFormat format{TextureFormat::Undefined};
    TextureViewDimension dimension{TextureViewDimension::Undefined};
    u32 base_mip_level{0};
    u32 mip_level_count{kMipLevelCountUndefined};
    u32 base_array_layer{0};
    u32 array_layer_count{kArrayLayerCountUndefined};
    TextureAspect aspect{TextureAspect::Undefined};
    TextureUsage usage{TextureUsage::None};
    std::string label{"TextureView"};
};

struct RenderBundleDesc final {
    void* next_in_chain{nullptr};
    std::string label{"RenderBundle"};
};

struct BindingResourceDesc final {
    void* next_in_chain{nullptr};
    Buffer* buffer{nullptr};
    u64 offset{0};
    u64 size{kWholeSize};
    Sampler* sampler{nullptr};
    TextureView* texture_view{nullptr};
};

struct SharedBufferMemoryBeginAccessDesc final {
    void* next_in_chain{nullptr};
};

struct SharedBufferMemoryEndAccessState final {
    void* next_in_chain{nullptr};
};

struct SharedBufferMemoryProperties final {
    void* next_in_chain{nullptr};
};

struct SharedTextureMemoryBeginAccessDesc final {
    void* next_in_chain{nullptr};
};

struct SharedTextureMemoryEndAccessState final {
    void* next_in_chain{nullptr};
};

struct SharedTextureMemoryProperties final {
    void* next_in_chain{nullptr};
};

struct SharedFenceExportInfo final {
    void* next_in_chain{nullptr};
};

} // namespace woki::rhi
