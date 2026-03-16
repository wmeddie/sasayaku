#pragma once
#ifdef _WIN32

#include <windows.h>
#include <gdiplus.h>
#include <vector>
#include <string>
#include <functional>

namespace sasayaku {

enum class OverlayState { Recording, Processing, Done };

class OverlayWindow {
public:
    OverlayWindow();
    ~OverlayWindow();

    bool create(HINSTANCE hInstance);
    void show();
    void hide();
    bool isVisible() const;

    // State
    void setState(OverlayState state);
    void setWaveformLevels(const std::vector<float>& levels);
    void setResultText(const std::string& text);
    std::string getResultText() const;
    void setModeName(const std::string& name);

    // Callbacks
    std::function<void()> onEscape;
    std::function<void()> onConfirm;  // Alt+Space in done state

    HWND getHwnd() const { return hwnd_; }

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    HWND hwnd_ = nullptr;
    HWND editHwnd_ = nullptr;
    OverlayState state_ = OverlayState::Recording;
    std::vector<float> waveformLevels_;
    std::string modeName_ = "Voice to Text";
    ULONG_PTR gdiplusToken_ = 0;

    void paint(HDC hdc);
    void paintWaveform(Gdiplus::Graphics& g, const Gdiplus::RectF& area);
    void paintStatusBar(Gdiplus::Graphics& g, const Gdiplus::RectF& area);
    void positionAtBottomCenter();
    void createEditControl();
    void updateEditVisibility();
};

} // namespace sasayaku

#endif
