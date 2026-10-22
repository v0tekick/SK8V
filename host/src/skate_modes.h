#pragma once
#include "skatev_runtime.h"
#include <cstdint>
#include <deque>
#include <vector>

// Modal Skate 3 tools. The host freezes the live Skate session while either
// tool is open, so playback and object placement cannot change trick physics.
namespace skatemodes {

class Modes {
public:
    enum class Tool { None, ObjectDropper, Replay };
    void Chords(std::uint16_t buttons, bool skating, bool menuOpen);
    bool Active() const { return tool_ != Tool::None; }
    Tool Current() const { return tool_; }
    void SetLogger(void (*log)(const char*)) { log_ = log; }
    bool HideReplayHud() const { return tool_ == Tool::Replay && hideReplayHud_; }
    void Record(const SvOutput& output, const float* bones, std::uint32_t boneCount,
                const SvBoardPose* board);
    void Tick(const SvPad& pad, int ped, int cam, float fovScale);
    void Close();
    void Reset();
    SvPad ResumePad(SvPad pad);
    void DrawMenuHints() const;

private:
    struct Frame {
        SvOutput output{};
        std::uint32_t gameMs = 0;
        std::vector<float> bones;
        SvBoardPose board{};
        bool hasBoard = false;
    };
    struct Placed { int entity = 0; std::uint32_t model = 0; };
    Tool tool_ = Tool::None;
    std::deque<Frame> frames_;
    std::vector<Placed> placed_;
    std::uint64_t lastRecordedTick_ = 0;
    std::uint16_t previousButtons_ = 0;
    bool objectChordWas_ = true, replayChordWas_ = true;
    bool releasePad_ = false;
    bool playing_ = false;
    enum class ReplayCamera { Follow, Free, Phone };
    ReplayCamera replayCamera_ = ReplayCamera::Follow;
    int filmer_ = 0, phone_ = 0;
    bool filmingAnimStarted_ = false;
    std::uint32_t lastFilmerMoveMs_ = 0;
    std::uint32_t filmerModel_ = 0, phoneModel_ = 0;
    bool hideReplayHud_ = false;
    bool keyboardHeld_[3]{};
    unsigned replayDuration_ = 2; // 30 seconds; Ctrl+Shift+D cycles 10 seconds through 2 minutes.
    float replayFov_ = 0.0f;
    int replayEffect_ = 0;
    float playhead_ = 0.0f;
    float previewX_ = 0.0f, previewY_ = 0.0f, previewZ_ = 0.0f;
    float previewHeading_ = 0.0f;
    float cameraYaw_ = 0.0f, cameraPitch_ = 0.0f;
    float cameraX_ = 0.0f, cameraY_ = 0.0f, cameraZ_ = 0.0f;
    int preview_ = 0, catalog_ = 0;
    std::uint32_t requestedModel_ = 0;
    std::uint32_t previewModelHash_ = 0;
    std::uint32_t lastTickMs_ = 0;
    std::uint32_t lastSpawnAttemptMs_ = 0;
    unsigned candidate_ = 0, spawnFailures_ = 0;
    const char* previewStatus_ = "Loading model";
    void (*log_)(const char*) = nullptr;
    void EnterObject(int ped);
    void ObjectTick(const SvPad& pad, std::uint16_t pressed, int ped, int cam, float dt);
    void ReplayTick(const SvPad& pad, std::uint16_t pressed, int ped, int cam, float dt, float fovScale);
    void MakePreview();
    void DeletePreview(bool resetCandidate = true);
    void DeleteFilmer();
    void PhoneCamera(const SvPad& pad, int cam, const SvOutput& frame, float dt);
public:
    bool IsPlacedObject(int entity) const;
    std::vector<int> PlacedObjects() const;
private:
    std::uint64_t ReplayStartTick() const;
};

} // namespace skatemodes
