#pragma once

struct IDXGISwapChain;

namespace replayfx {
// Called from the script thread; Present reads atomics on the render thread.
void Set(int effect, bool uiVisible = false);
void Present(IDXGISwapChain* chain);
void Stop();
}
