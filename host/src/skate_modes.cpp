#include "skate_modes.h"
#include "board_native.h"
#include "gta_audio.h"
#include "host_util.h"
#include "natives.h"
#include "pose_hook.h"
#include "replay_fx.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <windows.h>

namespace skatemodes {
namespace {
using gta::Call;
constexpr std::uint16_t Up = 0x0001, Left = 0x0004, Right = 0x0008;
constexpr std::uint16_t LB = 0x0100, RB = 0x0200, R3 = 0x0080;
constexpr std::uint16_t A = 0x1000, B = 0x2000, X = 0x4000, Y = 0x8000;
constexpr unsigned MaxFrames = 1800; // 30 seconds at Skate's 60 Hz
struct Model { const char* name; const char* candidates[3]; };
constexpr Model Catalog[] = {
    {"Small ramp", {"prop_mp_ramp_01", "stt_prop_ramp_jump_s", "prop_skate_flatramp"}},
    {"Medium ramp", {"prop_mp_ramp_02", "stt_prop_ramp_jump_m", "prop_skate_quartpipe"}},
    {"Large ramp", {"prop_mp_ramp_03", "stt_prop_ramp_jump_l", "prop_skate_halfpipe"}},
    {"Quarter pipe", {"stt_prop_stunt_bblock_qp", "stt_prop_stunt_bblock_qp2", "prop_skate_quartpipe"}},
    {"Funbox", {"stt_prop_stunt_bblock_mdm1", "stt_prop_stunt_bblock_sml1", "prop_skate_funbox"}},
    {"Rail", {"prop_skate_rail", "prop_fnclink_03e", "prop_fnclink_02a"}},
};
float Axis(std::int16_t value) {
    const float normalized = static_cast<float>(value) / 32767.0f;
    return std::copysign(std::max(0.0f, (std::abs(normalized) - 0.18f) / 0.82f), normalized);
}
void Label(const char* text, float x, float y, float scale = 0.32f) {
    gta::Text(text, x, y, scale, 235, 245, 255, 255, gta::Align::Left);
}
void CameraFrom(const SvOutput& o, int cam, float fovScale) {
    if (!cam || !o.camera_valid) return;
    Call<void>(gta::SET_CAM_COORD, cam, o.camera_position.x, o.camera_position.y, o.camera_position.z);
    Call<void>(gta::SET_CAM_ROT, cam, o.camera_rotation.x, o.camera_rotation.y, o.camera_rotation.z, 2);
    Call<void>(gta::SET_CAM_FOV, cam, std::clamp(o.camera_fov * fovScale, 20.0f, 120.0f));
}
} // namespace

void Modes::Chords(std::uint16_t buttons, bool skating, bool menuOpen) {
    const bool object = (buttons & (R3 | Right)) == (R3 | Right);
    const bool replay = (buttons & (R3 | Up)) == (R3 | Up);
    const bool openObject = object && !objectChordWas_;
    const bool openReplay = replay && !replayChordWas_;
    objectChordWas_ = object;
    replayChordWas_ = replay;
    if (!skating || menuOpen) return;
    if (openObject) {
        if (tool_ == Tool::ObjectDropper) Close();
        else { Close(); tool_ = Tool::ObjectDropper; previousButtons_ = buttons; lastTickMs_ = 0; }
    } else if (openReplay) {
        if (tool_ == Tool::Replay) Close();
        else if (!frames_.empty()) {
            Close(); tool_ = Tool::Replay;
            playhead_ = static_cast<float>(frames_.back().output.tick - frames_.front().output.tick);
            playing_ = freeCamera_ = false; previousButtons_ = buttons; lastTickMs_ = 0;
            replayFov_ = 0.0f;
        }
    }
}

void Modes::Record(const SvOutput& output, const float* bones, std::uint32_t boneCount,
                   const SvBoardPose* board) {
    if (tool_ != Tool::None || !output.tick || output.tick == lastRecordedTick_) return;
    Frame f;
    f.output = output;
    f.gameMs = static_cast<std::uint32_t>(Call<int>(gta::GET_GAME_TIMER));
    if (bones && boneCount && boneCount <= 512)
        f.bones.assign(bones, bones + static_cast<std::size_t>(boneCount) * 16);
    if (board && board->bone_count == 7) f.board = *board, f.hasBoard = true;
    frames_.push_back(std::move(f));
    lastRecordedTick_ = output.tick;
    while (frames_.size() > 1 &&
           (frames_.back().output.tick - frames_.front().output.tick > MaxFrames ||
            frames_.size() > MaxFrames)) frames_.pop_front();
}

void Modes::DeletePreview() {
    if (preview_ && Call<BOOL>(gta::DOES_ENTITY_EXIST, preview_))
        Call<void>(gta::DELETE_OBJECT, &preview_);
    preview_ = 0;
    previewModelHash_ = 0;
    candidate_ = spawnFailures_ = 0;
}

void Modes::Close() {
    if (tool_ == Tool::ObjectDropper) DeletePreview();
    if (tool_ == Tool::Replay) {
        gtaaudio::ReplayAudio(false, false, 0);
        replayfx::Set(0);
        // Replay owns the colour modifier while open.
        Call<void>(0x0F07E7745A236711ULL); // CLEAR_TIMECYCLE_MODIFIER
    }
    tool_ = Tool::None;
    playing_ = freeCamera_ = false;
    releasePad_ = true;
    lastTickMs_ = 0;
}

void Modes::Reset() {
    Close();
    frames_.clear();
    lastRecordedTick_ = 0;
}

SvPad Modes::ResumePad(SvPad pad) {
    if (!releasePad_) return pad;
    if (!pad.buttons && pad.left_trigger < 20 && pad.right_trigger < 20 &&
        std::abs(static_cast<int>(pad.left_x)) < 5000 && std::abs(static_cast<int>(pad.left_y)) < 5000 &&
        std::abs(static_cast<int>(pad.right_x)) < 5000 && std::abs(static_cast<int>(pad.right_y)) < 5000)
        releasePad_ = false;
    pad.buttons = 0;
    pad.left_trigger = pad.right_trigger = 0;
    pad.left_x = pad.left_y = pad.right_x = pad.right_y = 0;
    return pad;
}

void Modes::EnterObject(int ped) {
    const Vector3 p = Call<Vector3>(gta::GET_ENTITY_COORDS, ped, 1);
    const float yaw = Call<float>(gta::GET_ENTITY_HEADING, ped) * 0.0174532925f;
    previewX_ = p.x - std::sin(yaw) * 4.0f;
    previewY_ = p.y + std::cos(yaw) * 4.0f;
    previewZ_ = p.z;
    previewHeading_ = Call<float>(gta::GET_ENTITY_HEADING, ped);
    requestedModel_ = 0;
    preview_ = 0;
    lastSpawnAttemptMs_ = 0;
    if (log_) log_("Object Dropper opened; requesting selected GTA prop");
}

void Modes::MakePreview() {
    const auto& item = Catalog[catalog_];
    if (candidate_ >= std::size(item.candidates)) {
        previewStatus_ = "No placeable model for this item";
        return;
    }
    const char* modelName = item.candidates[candidate_];
    const std::uint32_t model = util::Joaat(modelName);
    if (requestedModel_ != model) {
        DeletePreview();
        requestedModel_ = model;
        lastSpawnAttemptMs_ = 0;
    }
    if (!Call<BOOL>(gta::IS_MODEL_IN_CDIMAGE, model) || !Call<BOOL>(gta::IS_MODEL_VALID, model)) {
        if (log_) { char line[160]; std::snprintf(line,sizeof(line),"Object Dropper: %s unavailable; trying next",modelName); log_(line); }
        ++candidate_;
        spawnFailures_ = 0;
        previewStatus_ = "Trying another model";
        return;
    }
    if (!Call<BOOL>(gta::HAS_MODEL_LOADED, model)) {
        previewStatus_ = "Loading model";
        Call<void>(gta::REQUEST_MODEL, model);
        return;
    }
    if (!preview_) {
        const auto now = GetTickCount();
        if (lastSpawnAttemptMs_ && now - lastSpawnAttemptMs_ < 750) return;
        lastSpawnAttemptMs_ = now;
        // The original skate props loaded but did not create in the user's
        // Story Mode session. Try the standard
        // object factory and a separate set of creator/stunt models first;
        // use NO_OFFSET only as a fallback on the same candidate.
        const int created = Call<int>(gta::CREATE_OBJECT, model, previewX_, previewY_, previewZ_, false, false, false, 0);
        preview_ = created && Call<BOOL>(gta::DOES_ENTITY_EXIST, created) ? created : 0;
        if (!preview_) {
            const int noOffset = Call<int>(gta::CREATE_OBJECT_NO_OFFSET, model, previewX_, previewY_, previewZ_, false, false, false, 0);
            preview_ = noOffset && Call<BOOL>(gta::DOES_ENTITY_EXIST, noOffset) ? noOffset : 0;
        }
        if (!preview_) {
            previewStatus_ = "Trying another model";
            if (log_) {
                char line[200];
                std::snprintf(line, sizeof(line), "Object Dropper: both object factories failed %s hash %08x at %.2f %.2f %.2f", modelName,
                              model, previewX_, previewY_, previewZ_);
                log_(line);
            }
            if (++spawnFailures_ >= 2) { ++candidate_; spawnFailures_ = 0; }
            return;
        }
        previewModelHash_ = model;
        spawnFailures_ = 0;
        if (log_) {
            char line[180];
            std::snprintf(line, sizeof(line), "Object Dropper: preview %s (%s) entity %d at %.2f %.2f %.2f", item.name, modelName,
                          preview_, previewX_, previewY_, previewZ_);
            log_(line);
        }
        Call<void>(gta::SET_ENTITY_AS_MISSION_ENTITY, preview_, true, false);
        Call<void>(gta::FREEZE_ENTITY_POSITION, preview_, true);
        Call<void>(gta::SET_ENTITY_COLLISION, preview_, false, false);
    }
    Call<void>(gta::SET_ENTITY_COORDS_NO_OFFSET, preview_, previewX_, previewY_, previewZ_, false, false, false);
    Call<void>(gta::SET_ENTITY_HEADING, preview_, previewHeading_);
    Call<void>(gta::SET_ENTITY_VISIBLE, preview_, true, false);
    previewStatus_ = Call<BOOL>(gta::DOES_ENTITY_EXIST, preview_) ? "Preview ready - press A to place" : "Preview vanished";
}

void Modes::ObjectTick(const SvPad& pad, std::uint16_t pressed, int ped, int cam, float dt) {
    if (pressed & B) { Close(); return; }
    if (pressed & Left) { catalog_ = (catalog_ + static_cast<int>(std::size(Catalog)) - 1) % static_cast<int>(std::size(Catalog)); DeletePreview(); candidate_ = spawnFailures_ = 0; }
    if (pressed & Right) { catalog_ = (catalog_ + 1) % static_cast<int>(std::size(Catalog)); DeletePreview(); candidate_ = spawnFailures_ = 0; }
    const Vector3 rot = cam ? Call<Vector3>(gta::GET_CAM_ROT, cam, 2) : Call<Vector3>(gta::GET_GAMEPLAY_CAM_ROT, 2);
    const float yaw = rot.z * 0.0174532925f;
    const float forward = Axis(pad.left_y) * 5.0f * dt;
    const float sideways = Axis(pad.left_x) * 5.0f * dt;
    previewX_ += -std::sin(yaw) * forward + std::cos(yaw) * sideways;
    previewY_ += std::cos(yaw) * forward + std::sin(yaw) * sideways;
    previewZ_ += (static_cast<float>(pad.right_trigger) - static_cast<float>(pad.left_trigger)) / 255.0f * 3.0f * dt;
    previewHeading_ += ((pad.buttons & RB ? 1.0f : 0.0f) - (pad.buttons & LB ? 1.0f : 0.0f)) * 90.0f * dt;
    MakePreview();
    if ((pressed & A) && preview_) {
        Call<void>(gta::SET_ENTITY_COLLISION, preview_, true, true);
        placed_.push_back({preview_, previewModelHash_});
        preview_ = 0;
        previewStatus_ = "Placed - press X to undo";
        if (log_) log_("Object Dropper: placed prop");
    }
    if ((pressed & X) && !placed_.empty()) {
        int last = placed_.back().entity;
        if (last && Call<BOOL>(gta::DOES_ENTITY_EXIST, last)) Call<void>(gta::DELETE_OBJECT, &last);
        placed_.pop_back();
    }
    char status[160];
    const char* activeModel = candidate_ < std::size(Catalog[catalog_].candidates)
        ? Catalog[catalog_].candidates[candidate_] : "unavailable";
    std::snprintf(status, sizeof(status), "OBJECT DROPPER  %s (%s)   placed %zu",
                  Catalog[catalog_].name, activeModel, placed_.size());
    Label(status, 0.28f, 0.75f, 0.42f);
    Label("D-pad left/right: object   Left stick: move   LT/RT: height   LB/RB: rotate", 0.28f, 0.79f);
    Label("A: place   X: undo   B or R3 + D-pad right: exit", 0.28f, 0.82f);
    Label(previewStatus_, 0.28f, 0.86f);
    (void)ped;
}

void Modes::ReplayTick(const SvPad& pad, std::uint16_t pressed, int ped, int cam, float dt, float fovScale) {
    if (pressed & B) { Close(); return; }
    if (frames_.empty()) { Close(); return; }
    if (pressed & A) playing_ = !playing_;
    if (pressed & Y) {
        freeCamera_ = !freeCamera_;
        if (freeCamera_) {
            const std::uint64_t wanted = frames_.front().output.tick + static_cast<std::uint64_t>(playhead_);
            const auto found = std::lower_bound(frames_.begin(), frames_.end(), wanted,
                [](const Frame& frame, std::uint64_t tick) { return frame.output.tick < tick; });
            const auto& o = (found == frames_.end() ? frames_.back() : *found).output;
            cameraX_ = o.camera_position.x, cameraY_ = o.camera_position.y, cameraZ_ = o.camera_position.z;
            cameraPitch_ = o.camera_rotation.x, cameraYaw_ = o.camera_rotation.z;
        }
    }
    if (pressed & Left) playhead_ -= 1.0f, playing_ = false;
    if (pressed & Right) playhead_ += 1.0f, playing_ = false;
    const float triggerDelta = (static_cast<float>(pad.right_trigger) - static_cast<float>(pad.left_trigger)) / 255.0f;
    const bool fovChord = (pad.buttons & R3) != 0;
    const float scrub = fovChord ? 0.0f : triggerDelta;
    if (std::abs(scrub) > 0.05f) playhead_ += scrub * 240.0f * dt, playing_ = false;
    else if (playing_) playhead_ += 60.0f * dt;
    const float durationTicks = static_cast<float>(frames_.back().output.tick - frames_.front().output.tick);
    playhead_ = std::clamp(playhead_, 0.0f, durationTicks);
    if (playhead_ >= durationTicks) playing_ = false;
    const std::uint64_t wanted = frames_.front().output.tick + static_cast<std::uint64_t>(playhead_);
    const auto found = std::lower_bound(frames_.begin(), frames_.end(), wanted,
        [](const Frame& recorded, std::uint64_t tick) { return recorded.output.tick < tick; });
    const Frame& frame = found == frames_.end() ? frames_.back() : *found;
    const SvOutput& o = frame.output;
    gtaaudio::ReplayAudio(true, playing_, frame.gameMs);
    Call<void>(gta::SET_ENTITY_COORDS_NO_OFFSET, ped, o.skater_position.x, o.skater_position.y, o.skater_position.z, true, true, false);
    Call<void>(gta::SET_ENTITY_HEADING, ped, o.skater_heading_degrees + 180.0f);
    if (!frame.bones.empty()) posehook::PublishPose(frame.bones.data(), static_cast<int>(frame.bones.size() / 16));
    boardnative::Tick(frame.hasBoard, frame.board);
    const bool chord = (GetAsyncKeyState(VK_CONTROL) & 0x8000) && (GetAsyncKeyState(VK_SHIFT) & 0x8000);
    const int keys[2] = {'H', 'E'};
    for (int i = 0; i < 2; ++i) {
        const bool down = chord && (GetAsyncKeyState(keys[i]) & 0x8000);
        if (down && !keyboardHeld_[i]) {
            if (i == 0) hideReplayHud_ = !hideReplayHud_;
            if (i == 1) replayEffect_ = (replayEffect_ + 1) % 5;
        }
        keyboardHeld_[i] = down;
    }
    float baseFov = replayFov_ ? replayFov_ :
        (replayEffect_ == 1 ? std::max(o.camera_fov * fovScale, 110.0f) :
         replayEffect_ == 4 ? std::max(o.camera_fov * fovScale, 115.0f) : o.camera_fov * fovScale);
    if (fovChord && std::abs(triggerDelta) > 0.03f)
        replayFov_ = baseFov = std::clamp(baseFov + triggerDelta * 55.0f * dt, 25.0f, 120.0f);
    if (!freeCamera_) CameraFrom(o, cam, baseFov / std::max(o.camera_fov, 0.01f));
    else {
        cameraYaw_ -= Axis(pad.right_x) * 110.0f * dt;
        cameraPitch_ = std::clamp(cameraPitch_ + Axis(pad.right_y) * 90.0f * dt, -85.0f, 85.0f);
        const float yaw = cameraYaw_ * 0.0174532925f;
        const float forward = Axis(pad.left_y) * 5.0f * dt;
        const float sideways = Axis(pad.left_x) * 5.0f * dt;
        cameraX_ += -std::sin(yaw) * forward + std::cos(yaw) * sideways;
        cameraY_ += std::cos(yaw) * forward + std::sin(yaw) * sideways;
        cameraZ_ += ((pad.buttons & RB ? 1.0f : 0.0f) - (pad.buttons & LB ? 1.0f : 0.0f)) * 3.0f * dt;
        Call<void>(gta::SET_CAM_COORD, cam, cameraX_, cameraY_, cameraZ_);
        Call<void>(gta::SET_CAM_ROT, cam, cameraPitch_, 0.0f, cameraYaw_, 2);
    }
    Call<void>(gta::SET_CAM_FOV, cam, baseFov);
    replayfx::Set(replayEffect_, !hideReplayHud_);
    if (replayEffect_ == 2) Call<void>(0x2C933ABF17A1DF41ULL, "scanline_cam");
    else if (replayEffect_ == 3) Call<void>(0x2C933ABF17A1DF41ULL, "NG_filmnoir_BW01");
    else Call<void>(0x0F07E7745A236711ULL);
    if (hideReplayHud_) return;
    // Opaque chrome also gives the Present shader a safe area to preserve:
    // camera effects cannot bend or crop the replay controls.
    Call<void>(gta::DRAW_RECT, 0.5f, 0.0425f, 1.0f, 0.085f, 9, 15, 24, 255, false);
    Call<void>(gta::DRAW_RECT, 0.5f, 0.9275f, 1.0f, 0.145f, 9, 15, 24, 255, false);
    Call<void>(gta::DRAW_RECT, 0.025f, 0.041f, 0.005f, 0.052f, 33, 156, 255, 255, false);
    gta::Text("REPLAY", 0.038f, 0.009f, 0.67f, 245, 250, 255, 255, gta::Align::Left);
    Label(playing_ ? "PLAYING" : "PAUSED", 0.212f, 0.034f, 0.30f);
    static const char* effects[] = {"CLEAN", "GOPRO", "VHS", "FILM NOIR", "DOORBELL"};
    char cameraInfo[112];
    std::snprintf(cameraInfo, sizeof(cameraInfo), "%s CAMERA    %s    FOV %.0f",
                  freeCamera_ ? "FREE" : "FOLLOW", effects[replayEffect_], baseFov);
    gta::Text(cameraInfo, 0.0f, 0.028f, 0.31f, 227, 240, 255, 255, gta::Align::Right, 0.975f);

    const float begin = 0.055f, end = 0.945f, span = end - begin;
    const float progress = durationTicks > 0.0f ? playhead_ / durationTicks : 0.0f;
    Call<void>(gta::DRAW_RECT, 0.5f, 0.895f, span, 0.004f, 74, 92, 112, 255, false);
    if (progress > 0.0f)
        Call<void>(gta::DRAW_RECT, begin + span*progress*0.5f, 0.895f, span*progress, 0.006f, 35, 157, 255, 255, false);
    for (int mark = 0; mark <= 30; ++mark) {
        const float x = begin + span*mark/30.0f;
        Call<void>(gta::DRAW_RECT, x, 0.882f, 0.001f, mark%5 ? 0.007f : 0.013f,
                   147, 164, 184, 210, false);
    }
    Call<void>(gta::DRAW_RECT, begin + span*progress, 0.894f, 0.004f, 0.034f, 246, 250, 255, 255, false);
    char times[96];
    std::snprintf(times, sizeof(times), "%02d:%04.1f  /  %02d:%04.1f",
                  static_cast<int>(playhead_/3600), std::fmod(playhead_/60.0f,60.0f),
                  static_cast<int>(durationTicks/3600), std::fmod(durationTicks/60.0f,60.0f));
    Label(times, begin, 0.909f, 0.31f);
    gta::Text("LT/RT SCRUB    RS + LT/RT FOV    A PLAY    Y CAMERA    B EXIT", 0.0f, 0.908f,
              0.29f, 224, 237, 248, 255, gta::Align::Right, 0.945f);
    Label("LEFT/RIGHT FRAME    CTRL+SHIFT+E EFFECT    CTRL+SHIFT+H HIDE UI", begin, 0.956f, 0.28f);
    gta::Text("FREE CAM: STICKS LOOK/MOVE  LB/RB HEIGHT", 0.0f, 0.956f, 0.28f,
              158, 188, 215, 255, gta::Align::Right, 0.945f);
}

void Modes::Tick(const SvPad& pad, int ped, int cam, float fovScale) {
    if (!Active()) return;
    const auto now = GetTickCount();
    const float dt = lastTickMs_ ? std::clamp(static_cast<float>(now - lastTickMs_) / 1000.0f, 0.0f, 0.1f) : 0.0f;
    if (tool_ == Tool::ObjectDropper && !lastTickMs_) EnterObject(ped);
    lastTickMs_ = now;
    const std::uint16_t pressed = pad.buttons & ~previousButtons_;
    previousButtons_ = pad.buttons;
    if (tool_ == Tool::ObjectDropper) ObjectTick(pad, pressed, ped, cam, dt);
    else ReplayTick(pad, pressed, ped, cam, dt, fovScale);
}

void Modes::DrawMenuHints() const {
    Label("RS + D-pad left: Skate now     RS + D-pad right: Object Dropper     RS + D-pad up: Replay", 0.02f, 0.975f, 0.25f);
}
} // namespace skatemodes
