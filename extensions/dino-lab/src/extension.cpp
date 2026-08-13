#include <woki/ecs/guest.hpp>
#include <woki/extension.hpp>
#include <woki/math/guest.hpp>

namespace {

using namespace woki;
using namespace math;

constexpr u32 kCapacity = 16;
constexpr u32 kStateMagic = 0x4f4e4944u; // DINO
constexpr u32 kStateVersion = 1;
constexpr u32 kRecordBytes = 28;
constexpr u32 kHeaderBytes = 16;

enum class Species : u32 { Triceratops, Velociraptor, Brachiosaurus };

struct Dinosaur final {
    u32 id{};
    Species species{};
    vec2<float> position{};
    vec2<float> velocity{};
    u32 age_ticks{};
};

constexpr StringView SpeciesName(Species species) noexcept {
    switch (species) {
        case Species::Triceratops:
            return "triceratops";
        case Species::Velociraptor:
            return "velociraptor";
        case Species::Brachiosaurus:
            return "brachiosaurus";
    }
    return "unknown";
}

constexpr Species ParseSpecies(StringView value) noexcept {
    if (value == "velociraptor")
        return Species::Velociraptor;
    if (value == "brachiosaurus")
        return Species::Brachiosaurus;
    return Species::Triceratops;
}

constexpr void WriteU32(u8* output, u32 value) noexcept {
    output[0] = static_cast<u8>(value);
    output[1] = static_cast<u8>(value >> 8u);
    output[2] = static_cast<u8>(value >> 16u);
    output[3] = static_cast<u8>(value >> 24u);
}

constexpr u32 ReadU32(const u8* input) noexcept {
    return static_cast<u32>(input[0]) | (static_cast<u32>(input[1]) << 8u) | (static_cast<u32>(input[2]) << 16u) | (static_cast<u32>(input[3]) << 24u);
}

constexpr void WriteFloat(u8* output, float value) noexcept {
    WriteU32(output, __builtin_bit_cast(u32, value));
}

constexpr float ReadFloat(const u8* input) noexcept {
    return __builtin_bit_cast(float, ReadU32(input));
}

class DinoLab final {
public:
    Status OnAttach() noexcept {
        slog::Info("Dino Lab is opening");
        LogPaths();
        LoadPreferences();
        if (!LoadPark()) {
            (void)Spawn(default_species_, false);
            (void)Spawn(Species::Brachiosaurus, false);
        }
        slog::Info("Dino Lab ready with ", count_, " dinosaur(s)");
        return Status::Success();
    }

    void OnUpdate(f64 delta_ms) noexcept {
        const float step = static_cast<float>(delta_ms) * static_cast<float>(speed_percent_) / 100'000.0f;
        for (u32 index = 0; index < count_; ++index) {
            Dinosaur* dinosaur = dinosaurs_.Get(entities_[index]);
            if (dinosaur == nullptr)
                continue;
            dinosaur->position += dinosaur->velocity * step;
            Bounce(dinosaur->position.x, dinosaur->velocity.x, habitat_width_);
            Bounce(dinosaur->position.y, dinosaur->velocity.y, habitat_height_);
            ++dinosaur->age_ticks;
        }
    }

    void OnEvent(events::Event& event) noexcept {
        if (event.IsNamed()) {
            if (event.Topic() == "woki.dinolab.feed") {
                for (u32 index = 0; index < count_; ++index) {
                    if (Dinosaur* dinosaur = dinosaurs_.Get(entities_[index]))
                        dinosaur->velocity *= 1.1f;
                }
                slog::Info("The herd has been fed");
            }
            return;
        }

        events::EventDispatcher dispatcher{event};
        dispatcher.Dispatch<events::WindowResizedEvent>([this](events::WindowResizedEvent value) noexcept {
            habitat_width_ = static_cast<float>(value.width == 0u ? 1u : value.width);
            habitat_height_ = static_cast<float>(value.height == 0u ? 1u : value.height);
        });
        dispatcher.Dispatch<events::WindowMovedEvent>([](events::WindowMovedEvent value) noexcept { slog::Debug("Window moved to ", value.x, ',', value.y); });
        dispatcher.Dispatch<events::WindowClosedEvent>([](events::WindowClosedEvent) noexcept { slog::Info("Window closed"); });
        dispatcher.Dispatch<events::WindowFocusedEvent>([](events::WindowFocusedEvent) noexcept { slog::Debug("Window focused"); });
        dispatcher.Dispatch<events::WindowLostFocusEvent>([](events::WindowLostFocusEvent) noexcept { slog::Debug("Window focus lost"); });
        dispatcher.Dispatch<events::WindowMinimizedEvent>([](events::WindowMinimizedEvent) noexcept { slog::Debug("Window minimized"); });
        dispatcher.Dispatch<events::WindowMaximizedEvent>([](events::WindowMaximizedEvent) noexcept { slog::Debug("Window maximized"); });
        dispatcher.Dispatch<events::WindowRestoredEvent>([](events::WindowRestoredEvent) noexcept { slog::Debug("Window restored"); });
        dispatcher.Dispatch<events::KeyPressedEvent>([this](events::KeyPressedEvent value) noexcept {
            slog::Info("Key pressed: ", value.key, " repeat ", value.repeat_count);
            if (count_ != 0u) {
                if (Dinosaur* dinosaur = dinosaurs_.Get(entities_[0]))
                    dinosaur->velocity.x += 0.25f;
            }
        });
        dispatcher.Dispatch<events::KeyReleasedEvent>([](events::KeyReleasedEvent value) noexcept { slog::Debug("Key released: ", value.key); });
        dispatcher.Dispatch<events::TextInputEvent>([](events::TextInputEvent value) noexcept { slog::Debug("Text input bytes: ", value.text_size); });
        dispatcher.Dispatch<events::ScrolledEvent>([](events::ScrolledEvent) noexcept { slog::Debug("Pointer scrolled"); });
        dispatcher.Dispatch<events::PointerDownEvent>([](events::PointerDownEvent value) noexcept { slog::Debug("Pointer down: ", static_cast<u32>(value.pointer_id)); });
        dispatcher.Dispatch<events::PointerUpEvent>([](events::PointerUpEvent value) noexcept { slog::Debug("Pointer up: ", static_cast<u32>(value.pointer_id)); });
        dispatcher.Dispatch<events::PointerCancelEvent>([](events::PointerCancelEvent value) noexcept { slog::Debug("Pointer canceled: ", static_cast<u32>(value.pointer_id)); });
        dispatcher.Dispatch<events::PointerEnteredEvent>([](events::PointerEnteredEvent value) noexcept { slog::Debug("Pointer entered: ", static_cast<u32>(value.pointer_id)); });
        dispatcher.Dispatch<events::PointerLeftEvent>([](events::PointerLeftEvent value) noexcept { slog::Debug("Pointer left: ", static_cast<u32>(value.pointer_id)); });
        dispatcher.Dispatch<events::PinchEvent>([](events::PinchEvent) noexcept { slog::Debug("Pointer pinch"); });
        dispatcher.Dispatch<events::GamepadButtonChangedEvent>([](events::GamepadButtonChangedEvent value) noexcept { slog::Debug("Gamepad button: ", value.button); });
        dispatcher.Dispatch<events::AppSuspendEvent>([](events::AppSuspendEvent) noexcept { slog::Info("Application suspended"); });
        dispatcher.Dispatch<events::AppResumeEvent>([](events::AppResumeEvent) noexcept { slog::Info("Application resumed"); });
        dispatcher.Dispatch<events::AppShutdownEvent>([this](events::AppShutdownEvent) noexcept { (void)SavePark(); });
    }

    Status OnCommand(const extension::Command& command) noexcept {
        if (command.Id() == "woki.dinolab.spawn") {
            Species species = default_species_;
            if (!command.Payload().Empty())
                species = static_cast<Species>(static_cast<u32>(command.Payload().Data()[0]) % 3u);
            return Spawn(species, true);
        }
        if (command.Id() == "woki.dinolab.roar") {
            Roar();
            AppendAudit(2u);
            return Status::Success();
        }
        if (command.Id() == "woki.dinolab.status") {
            LogStatus();
            return Status::Success();
        }
        if (command.Id() == "woki.dinolab.save")
            return SavePark();
        if (command.Id() == "woki.dinolab.reset") {
            ResetPark();
            AppendAudit(3u);
            return SavePark();
        }
        if (command.Id() == "woki.dinolab.speed") {
            CycleSpeed();
            return Status::Success();
        }
        return Status::NotFound();
    }

    void OnDetach() noexcept {
        (void)SavePark();
        ResetPark();
        slog::Info("Dino Lab closed safely");
    }

private:
    static void Bounce(float& position, float& velocity, float boundary) noexcept {
        if (position < 0.0f) {
            position = 0.0f;
            velocity = -velocity;
        } else if (position > boundary) {
            position = boundary;
            velocity = -velocity;
        }
    }

    void LogPaths() noexcept {
        StringBuffer<WOKI_EXT_MAX_PATH_LEN> data;
        StringBuffer<WOKI_EXT_MAX_PATH_LEN> cache;
        const Status data_status = paths::Data(data);
        const Status cache_status = paths::Cache(cache);
        if (data_status)
            slog::Debug("Dino data path: ", data.View());
        if (cache_status)
            slog::Debug("Dino cache path: ", cache.View());
    }

    void LoadPreferences() noexcept {
        StringBuffer<32> species;
        const Status species_status = config::Get("default_species", species);
        if (species_status)
            default_species_ = ParseSpecies(species.View());
        else if (species_status.Code() == WOKI_EXT_NOT_FOUND)
            (void)config::Set("default_species", SpeciesName(default_species_));
        else
            slog::Error("Could not read default species configuration");

        StringBuffer<8> speed;
        const Status speed_status = config::Get("simulation_speed", speed);
        if (speed_status) {
            if (speed.View() == "50")
                speed_percent_ = 50u;
            else if (speed.View() == "200")
                speed_percent_ = 200u;
        } else if (speed_status.Code() == WOKI_EXT_NOT_FOUND) {
            (void)config::Set("simulation_speed", "100");
        }
    }

    Status Spawn(Species species, bool announce) noexcept {
        if (count_ == kCapacity) {
            slog::Warn("The dinosaur park is full");
            return Status::NoSpace();
        }
        const guest::Entity entity = registry_.Create();
        if (!entity)
            return Status::NoSpace();
        const float direction = (next_id_ & 1u) == 0u ? -1.0f : 1.0f;
        Dinosaur dinosaur{
            .id = next_id_++,
            .species = species,
            .position = {static_cast<float>(count_ * 3u), static_cast<float>(count_ * 2u)},
            .velocity = {direction * (species == Species::Velociraptor ? 4.0f : 1.5f), species == Species::Brachiosaurus ? 0.5f : 1.0f},
        };
        Dinosaur* stored = dinosaurs_.Emplace(entity, dinosaur);
        if (stored == nullptr) {
            (void)registry_.Destroy(entity);
            return Status::Error();
        }
        entities_[count_++] = entity;
        if (announce) {
            slog::Info("Spawned ", SpeciesName(species), " #", stored->id);
            EmitDinosaurEvent("woki.dinolab.spawned", 1u, *stored);
            AppendAudit(1u);
        }
        return Status::Success();
    }

    void Roar() noexcept {
        if (count_ == 0u) {
            slog::Warn("There are no dinosaurs to roar");
            return;
        }
        for (u32 index = 0; index < count_; ++index) {
            if (const Dinosaur* dinosaur = dinosaurs_.Get(entities_[index]))
                slog::Info(SpeciesName(dinosaur->species), " #", dinosaur->id, " says ROAR!");
        }
        if (const Dinosaur* dinosaur = dinosaurs_.Get(entities_[0]))
            EmitDinosaurEvent("woki.dinolab.roared", 2u, *dinosaur);
    }

    void LogStatus() noexcept {
        slog::Info("Dino Lab status: ", count_, '/', kCapacity, " dinosaurs at ", speed_percent_, "% speed");
        for (u32 index = 0; index < count_; ++index) {
            if (const Dinosaur* dinosaur = dinosaurs_.Get(entities_[index]))
                slog::Info("#", dinosaur->id, ' ', SpeciesName(dinosaur->species), " age ", dinosaur->age_ticks, " position ", static_cast<i32>(dinosaur->position.x), ',', static_cast<i32>(dinosaur->position.y));
        }
    }

    void EmitDinosaurEvent(StringView topic, u32 local_type, const Dinosaur& dinosaur) noexcept {
        u8 payload[8]{};
        WriteU32(payload, dinosaur.id);
        WriteU32(payload + 4u, static_cast<u32>(dinosaur.species));
        (void)events::Emit(topic, {payload, 8u});
        (void)events::Emit(WOKI_EXT_EVENT_EXTENSION_ID(local_type), {payload, 8u});
    }

    void AppendAudit(u8 action) noexcept {
        const u8 record[2]{action, static_cast<u8>(count_)};
        (void)storage::Append("audit.bin", {record, 2u});
    }

    Status SavePark() noexcept {
        u8 state[kHeaderBytes + kCapacity * kRecordBytes]{};
        WriteU32(state, kStateMagic);
        WriteU32(state + 4u, kStateVersion);
        WriteU32(state + 8u, count_);
        WriteU32(state + 12u, next_id_);
        for (u32 index = 0; index < count_; ++index) {
            const Dinosaur* dinosaur = dinosaurs_.Get(entities_[index]);
            if (dinosaur == nullptr)
                continue;
            u8* record = state + kHeaderBytes + index * kRecordBytes;
            WriteU32(record, dinosaur->id);
            WriteU32(record + 4u, static_cast<u32>(dinosaur->species));
            WriteFloat(record + 8u, dinosaur->position.x);
            WriteFloat(record + 12u, dinosaur->position.y);
            WriteFloat(record + 16u, dinosaur->velocity.x);
            WriteFloat(record + 20u, dinosaur->velocity.y);
            WriteU32(record + 24u, dinosaur->age_ticks);
        }
        const Status status = storage::Write("park.bin", {state, kHeaderBytes + count_ * kRecordBytes});
        if (status)
            slog::Debug("Saved ", count_, " dinosaur(s)");
        else
            slog::Error("Could not save dinosaur park");
        return status;
    }

    bool LoadPark() noexcept {
        u8 state[kHeaderBytes + kCapacity * kRecordBytes]{};
        MutableBytes input{state, sizeof(state)};
        const Status status = storage::Read("park.bin", input);
        if (status.Code() == WOKI_EXT_NOT_FOUND) {
            slog::Debug("No saved dinosaur park found");
            return false;
        }
        if (!status || input.Size() < kHeaderBytes || ReadU32(state) != kStateMagic || ReadU32(state + 4u) != kStateVersion) {
            slog::Warn("Saved dinosaur park is invalid");
            return false;
        }
        const u32 stored_count = ReadU32(state + 8u);
        if (stored_count > kCapacity || input.Size() != kHeaderBytes + stored_count * kRecordBytes) {
            slog::Warn("Saved dinosaur park has an invalid size");
            return false;
        }
        ResetPark();
        for (u32 index = 0; index < stored_count; ++index) {
            const u8* record = state + kHeaderBytes + index * kRecordBytes;
            const Species species = static_cast<Species>(ReadU32(record + 4u) % 3u);
            if (!Spawn(species, false)) {
                ResetPark();
                return false;
            }
            Dinosaur* dinosaur = dinosaurs_.Get(entities_[index]);
            dinosaur->id = ReadU32(record);
            dinosaur->position = {ReadFloat(record + 8u), ReadFloat(record + 12u)};
            dinosaur->velocity = {ReadFloat(record + 16u), ReadFloat(record + 20u)};
            dinosaur->age_ticks = ReadU32(record + 24u);
        }
        next_id_ = ReadU32(state + 12u);
        slog::Info("Loaded ", count_, " saved dinosaur(s)");
        return true;
    }

    void ResetPark() noexcept {
        for (u32 index = 0; index < count_; ++index) {
            (void)dinosaurs_.Remove(entities_[index]);
            (void)registry_.Destroy(entities_[index]);
            entities_[index] = {};
        }
        count_ = 0u;
        next_id_ = 1u;
    }

    void CycleSpeed() noexcept {
        if (speed_percent_ == 50u) {
            speed_percent_ = 100u;
            (void)config::Set("simulation_speed", "100");
        } else if (speed_percent_ == 100u) {
            speed_percent_ = 200u;
            (void)config::Set("simulation_speed", "200");
        } else {
            speed_percent_ = 50u;
            (void)config::Set("simulation_speed", "50");
        }
        slog::Info("Simulation speed changed to ", speed_percent_, '%');
    }

    guest::Registry<kCapacity> registry_;
    guest::ComponentPool<Dinosaur, kCapacity> dinosaurs_;
    guest::Entity entities_[kCapacity]{};
    u32 count_{};
    u32 next_id_{1u};
    u32 speed_percent_{100u};
    Species default_species_{Species::Triceratops};
    float habitat_width_{1280.0f};
    float habitat_height_{720.0f};
};

} // namespace

WOKI_EXTENSION(DinoLab)
