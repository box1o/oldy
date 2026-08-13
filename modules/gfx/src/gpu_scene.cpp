#include <algorithm>
#include <bit>
#include <cstring>

#include <woki/gfx/advanced/gpu_scene.hpp>

namespace woki::gfx {
namespace {

void PackTransform(std::array<f32, 12>& output, const math::mat4f& matrix, const math::vec3f origin) noexcept {
    for (u32 row = 0; row < 3; ++row) {
        output[row * 4] = matrix(row, 0);
        output[row * 4 + 1] = matrix(row, 1);
        output[row * 4 + 2] = matrix(row, 2);
        output[row * 4 + 3] = matrix(row, 3) - origin[row];
    }
}

template <typename T>
std::vector<std::byte> CopyBytes(const std::span<const T> values, const u64 offset, const u64 size) {
    std::vector<std::byte> bytes(static_cast<size_t>(size), std::byte{});
    const u64 available = static_cast<u64>(values.size_bytes());
    if (offset < available) {
        const u64 copy_size = std::min(size, available - offset);
        std::memcpy(bytes.data(), reinterpret_cast<const std::byte*>(values.data()) + offset, static_cast<size_t>(copy_size));
    }
    return bytes;
}

} // namespace

GpuInstanceRecord PackGpuInstance(const RenderWorldSnapshot& world, const u32 dense_index, const math::vec3f camera_origin, const u32 gpu_version) noexcept {
    const auto& objects = world.ObjectData();
    GpuInstanceRecord result;
    PackTransform(result.current_transform, objects.transforms[dense_index], camera_origin);
    PackTransform(result.previous_transform, objects.previous_transforms[dense_index], camera_origin);
    const auto& bounds = objects.bounds[dense_index];
    result.bounding_sphere = {bounds.center.x - camera_origin.x, bounds.center.y - camera_origin.y, bounds.center.z - camera_origin.z, bounds.radius};
    result.resources = {
        objects.meshes[dense_index].Index(),
        objects.materials[dense_index].Index(),
        objects.palettes[dense_index].Index(),
        gpu_version,
    };
    result.identity = {
        objects.ids[dense_index].Index(),
        objects.ids[dense_index].Generation(),
        static_cast<u32>(objects.flags[dense_index]),
        0,
    };
    return result;
}

GpuLightRecord PackGpuLight(const RenderLightData& light, const math::vec3f camera_origin) noexcept {
    GpuLightRecord result;
    result.position_range = {light.position.x - camera_origin.x, light.position.y - camera_origin.y, light.position.z - camera_origin.z, light.range};
    result.direction_outer_cos = {light.direction.x, light.direction.y, light.direction.z, light.spot_outer_cos};
    result.color_intensity = {light.color.x, light.color.y, light.color.z, light.intensity};
    result.type_flags_mask = {static_cast<u32>(light.type), std::bit_cast<u32>(light.spot_inner_cos), static_cast<u32>(light.visibility_mask), static_cast<u32>(light.visibility_mask >> 32U)};
    return result;
}

GpuScene::GpuScene(BufferPool& pool, UploadScheduler& uploads, const u32 initial_capacity, const f32 whole_upload_threshold)
    : pool_(&pool),
      uploads_(&uploads),
      instance_dirty_(0, whole_upload_threshold),
      light_dirty_(0, whole_upload_threshold),
      initial_capacity_(std::max(1U, initial_capacity)),
      whole_upload_threshold_(std::clamp(whole_upload_threshold, 0.0F, 1.0F)) {}

GpuScene::~GpuScene() {
    if (pool_ == nullptr)
        return;
    for (const auto allocation : {instances_allocation_.allocation, transforms_allocation_.allocation, lights_allocation_.allocation, materials_allocation_.allocation})
        if (allocation.IsValid())
            static_cast<void>(pool_->Free(allocation, last_used_));
}

Result<void> GpuScene::EnsureCapacity(const u32 instances, const u32 lights) {
    const u32 required_instances = std::max(instances, initial_capacity_);
    const u32 required_lights = std::max(lights, std::max(1U, initial_capacity_ / 4U));
    if (required_instances <= capacity_ && required_lights <= light_capacity_)
        return Ok();
    const u32 new_capacity = std::max(required_instances, capacity_ == 0 ? initial_capacity_ : capacity_ * 2U);
    const u32 new_light_capacity = std::max(required_lights, light_capacity_ == 0 ? std::max(1U, initial_capacity_ / 4U) : light_capacity_ * 2U);

    std::array<BufferSlice, 4> fresh;
    auto allocate = [&](const size_t index, const u64 size) -> Result<void> {
        auto result = pool_->Allocate(size, 16);
        if (!result)
            return Err(std::move(result).error());
        fresh[index] = *result;
        return Ok();
    };
    auto allocated = allocate(0, static_cast<u64>(new_capacity) * sizeof(GpuInstanceRecord));
    if (allocated)
        allocated = allocate(1, static_cast<u64>(new_capacity) * 96U);
    if (allocated)
        allocated = allocate(2, static_cast<u64>(new_light_capacity) * sizeof(GpuLightRecord));
    if (allocated)
        allocated = allocate(3, static_cast<u64>(new_capacity) * 16U);
    if (!allocated) {
        for (const auto& slice : fresh)
            if (slice.allocation.IsValid())
                static_cast<void>(pool_->Free(slice.allocation));
        return Err(std::move(allocated).error());
    }
    for (const auto& old : {instances_allocation_, transforms_allocation_, lights_allocation_, materials_allocation_})
        if (old.allocation.IsValid())
            TRY_VOID(pool_->Free(old.allocation, last_used_));
    instances_allocation_ = fresh[0];
    transforms_allocation_ = fresh[1];
    lights_allocation_ = fresh[2];
    materials_allocation_ = fresh[3];
    capacity_ = new_capacity;
    light_capacity_ = new_light_capacity;
    ++allocation_version_;
    instance_dirty_ = DirtyRangeSet(static_cast<u64>(capacity_) * sizeof(GpuInstanceRecord), whole_upload_threshold_);
    light_dirty_ = DirtyRangeSet(static_cast<u64>(light_capacity_) * sizeof(GpuLightRecord), whole_upload_threshold_);
    instance_dirty_.Mark(0, static_cast<u64>(instances_.Size()) * sizeof(GpuInstanceRecord));
    light_dirty_.Mark(0, static_cast<u64>(lights_.size()) * sizeof(GpuLightRecord));
    return Ok();
}

void GpuScene::Remove(const RenderObjectId id) {
    const u32 dense = instances_.DenseIndex(id);
    const u32 last = static_cast<u32>(instances_.Size() - 1);
    retired_.push_back({id, last_used_});
    static_cast<void>(instances_.Remove(id));
    mappings_[id.Index()].generation = 0;
    if (dense != last)
        instance_dirty_.Mark(static_cast<u64>(dense) * sizeof(GpuInstanceRecord), sizeof(GpuInstanceRecord));
}

Result<void> GpuScene::Synchronize(const RenderWorldSnapshot& world, const math::vec3f camera_origin) {
    if (device_lost_)
        return Err(ErrorCode::GraphicsDeviceLost, "GPU scene device is lost");
    TRY_VOID(EnsureCapacity(static_cast<u32>(world.ObjectData().ids.size()), static_cast<u32>(world.Lights().size())));
    const bool origin_changed = !has_camera_origin_ || camera_origin.x != camera_origin_.x || camera_origin.y != camera_origin_.y || camera_origin.z != camera_origin_.z;
    camera_origin_ = camera_origin;
    has_camera_origin_ = true;
    std::vector<bool> seen(mappings_.size(), false);
    const auto& objects = world.ObjectData();
    for (size_t source_index = 0; source_index < objects.ids.size(); ++source_index) {
        const u32 index = static_cast<u32>(source_index);
        const auto id = objects.ids[index];
        if (id.Index() >= mappings_.size()) {
            mappings_.resize(static_cast<size_t>(id.Index()) + 1);
            seen.resize(mappings_.size(), false);
        }
        auto& mapping = mappings_[id.Index()];
        if (!instances_.Contains(id)) {
            if (mapping.generation != 0)
                Remove(RenderObjectId::Create(id.Index(), mapping.generation));
            mapping = Mapping{id.Generation(), next_mapping_version_++, objects.versions[index]};
            static_cast<void>(instances_.Emplace(id, PackGpuInstance(world, index, camera_origin, mapping.version)));
            instance_dirty_.Mark(static_cast<u64>(instances_.DenseIndex(id)) * sizeof(GpuInstanceRecord), sizeof(GpuInstanceRecord));
        } else if (origin_changed || mapping.source_version != objects.versions[index]) {
            mapping.version = next_mapping_version_++;
            mapping.source_version = objects.versions[index];
            instances_.Get(id) = PackGpuInstance(world, index, camera_origin, mapping.version);
            instance_dirty_.Mark(static_cast<u64>(instances_.DenseIndex(id)) * sizeof(GpuInstanceRecord), sizeof(GpuInstanceRecord));
        }
        seen[id.Index()] = true;
    }
    for (u32 dense = static_cast<u32>(instances_.Size()); dense > 0; --dense) {
        const auto id = instances_.Handles()[dense - 1];
        if (id.Index() >= seen.size() || !seen[id.Index()])
            Remove(id);
    }

    lights_.clear();
    lights_.reserve(world.Lights().size());
    for (const auto& light : world.Lights())
        lights_.push_back(PackGpuLight(light, camera_origin));
    light_dirty_.Mark(0, static_cast<u64>(lights_.size()) * sizeof(GpuLightRecord));
    return Ok();
}

Result<void> GpuScene::UploadDirty() {
    if (device_lost_)
        return Err(ErrorCode::GraphicsDeviceLost, "GPU scene device is lost");
    std::vector<BufferUploadRequest> requests;
    requests.reserve(instance_dirty_.Ranges().size() + light_dirty_.Ranges().size());
    for (const auto& range : instance_dirty_.Ranges())
        requests.push_back({.target = pool_->SharedBuffer(),
            .offset = instances_allocation_.offset + range.offset,
            .bytes = CopyBytes(std::span<const GpuInstanceRecord>(instances_.Values()), range.offset, range.size),
            .publication = {}});
    for (const auto& range : light_dirty_.Ranges())
        requests
            .push_back({.target = pool_->SharedBuffer(), .offset = lights_allocation_.offset + range.offset, .bytes = CopyBytes(std::span<const GpuLightRecord>(lights_), range.offset, range.size), .publication = {}});
    if (!requests.empty())
        TRY_VOID(uploads_->EnqueueBatch(std::move(requests)));
    instance_dirty_.Clear();
    light_dirty_.Clear();
    return Ok();
}

std::optional<GpuSceneInstance> GpuScene::Resolve(const RenderObjectId id) const noexcept {
    if (!instances_.Contains(id))
        return std::nullopt;
    return GpuSceneInstance(instances_.DenseIndex(id), mappings_[id.Index()].version);
}

void GpuScene::MarkUsed(const rhi::SubmissionTicket submission) noexcept {
    if (submission > last_used_)
        last_used_ = submission;
    pool_->MarkUsed(submission);
}

void GpuScene::Collect(const rhi::SubmissionEpoch completed) {
    std::erase_if(retired_, [&](const Retired& retired) { return !retired.safe_after.IsValid() || completed.HasReached(retired.safe_after); });
}

void GpuScene::MarkDeviceLost() noexcept {
    device_lost_ = true;
    instances_allocation_ = {};
    transforms_allocation_ = {};
    lights_allocation_ = {};
    materials_allocation_ = {};
}

GpuSceneStats GpuScene::Stats() const noexcept {
    return {static_cast<u32>(instances_.Size()), static_cast<u32>(lights_.size()), capacity_, allocation_version_, retired_.size(), device_lost_};
}

GpuSceneTables GpuScene::Tables() const noexcept {
    if (device_lost_)
        return {};
    return {instances_allocation_, transforms_allocation_, lights_allocation_, materials_allocation_, allocation_version_};
}

} // namespace woki::gfx
