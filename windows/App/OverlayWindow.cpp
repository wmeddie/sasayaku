#ifdef _WIN32

#include "OverlayWindow.hpp"
#include "../Platform/WinUtils.hpp"
#include <cmath>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

namespace sasayaku {

static constexpr int OVERLAY_WIDTH = 580;
static constexpr int OVERLAY_HEIGHT = 140;
static constexpr int STATUS_BAR_HEIGHT = 32;
static constexpr int TIMER_ID = 1;

OverlayWindow::OverlayWindow() {}

OverlayWindow::~OverlayWindow() {
    if (gdiplusToken_) {
        Gdiplus::GdiplusShutdown(gdiplusToken_);
    }
    if (hwnd_) {
        DestroyWindow(hwnd_);
    }
}

bool OverlayWindow::create(HINSTANCE hInstance) {
    // Init GDI+
    Gdiplus::GdiplusStartupInput gdiplusInput;
    Gdiplus::GdiplusStartup(&gdiplusToken_, &gdiplusInput, nullptr);

    // Register window class
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"SasayakuOverlay";
    wc.hbrBackground = nullptr;
    wc.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassExW(&wc);

    hwnd_ = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        L"SasayakuOverlay", L"Sasayaku",
        WS_POPUP,
        0, 0, OVERLAY_WIDTH, OVERLAY_HEIGHT,
        nullptr, nullptr, hInstance, this
    );

    if (!hwnd_) return false;

    // Semi-transparent background
    SetLayeredWindowAttributes(hwnd_, 0, 230, LWA_ALPHA);

    createEditControl();
    positionAtBottomCenter();
    return true;
}

void OverlayWindow::createEditControl() {
    editHwnd_ = CreateWindowExW(
        0, L"EDIT", L"",
        WS_CHILD | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
        16, 12, OVERLAY_WIDTH - 32, OVERLAY_HEIGHT - STATUS_BAR_HEIGHT - 24,
        hwnd_, nullptr, GetModuleHandle(nullptr), nullptr
    );

    // Set font
    HFONT hFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    SendMessage(editHwnd_, WM_SETFONT, (WPARAM)hFont, TRUE);
}

void OverlayWindow::show() {
    positionAtBottomCenter();
    updateEditVisibility();
    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
    InvalidateRect(hwnd_, nullptr, TRUE);
    SetTimer(hwnd_, TIMER_ID, 50, nullptr);  // 50ms refresh for waveform
}

void OverlayWindow::hide() {
    KillTimer(hwnd_, TIMER_ID);
    ShowWindow(hwnd_, SW_HIDE);
}

bool OverlayWindow::isVisible() const {
    return hwnd_ && IsWindowVisible(hwnd_);
}

void OverlayWindow::setState(OverlayState state) {
    state_ = state;
    updateEditVisibility();
    if (state == OverlayState::Done && editHwnd_) {
        SetFocus(editHwnd_);
        // Select all text for easy replacement
        SendMessage(editHwnd_, EM_SETSEL, 0, -1);
    }
    InvalidateRect(hwnd_, nullptr, TRUE);
}

void OverlayWindow::setWaveformLevels(const std::vector<float>& levels) {
    waveformLevels_ = levels;
    if (state_ == OverlayState::Recording || state_ == OverlayState::Processing) {
        InvalidateRect(hwnd_, nullptr, TRUE);
    }
}

void OverlayWindow::setResultText(const std::string& text) {
    if (editHwnd_) {
        std::wstring wtext = utf8_to_wide(text);
        SetWindowTextW(editHwnd_, wtext.c_str());
    }
}

std::string OverlayWindow::getResultText() const {
    if (!editHwnd_) return "";
    int len = GetWindowTextLengthW(editHwnd_);
    if (len == 0) return "";
    std::wstring wtext(len + 1, 0);
    GetWindowTextW(editHwnd_, &wtext[0], len + 1);
    wtext.resize(len);
    return wide_to_utf8(wtext);
}

void OverlayWindow::setModeName(const std::string& name) {
    modeName_ = name;
    InvalidateRect(hwnd_, nullptr, TRUE);
}

void OverlayWindow::updateEditVisibility() {
    if (editHwnd_) {
        ShowWindow(editHwnd_, state_ == OverlayState::Done ? SW_SHOW : SW_HIDE);
    }
}

void OverlayWindow::positionAtBottomCenter() {
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenW - OVERLAY_WIDTH) / 2;
    int y = screenH - OVERLAY_HEIGHT - 80;
    SetWindowPos(hwnd_, HWND_TOPMOST, x, y, OVERLAY_WIDTH, OVERLAY_HEIGHT, SWP_NOACTIVATE);
}

LRESULT CALLBACK OverlayWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    OverlayWindow* self = nullptr;

    if (msg == WM_NCCREATE) {
        auto cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        self = static_cast<OverlayWindow*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<OverlayWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (self) {
        switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            self->paint(hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_TIMER:
            if (wParam == TIMER_ID) {
                InvalidateRect(hwnd, nullptr, TRUE);
            }
            return 0;
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE && self->onEscape) {
                self->onEscape();
                return 0;
            }
            break;
        }
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void OverlayWindow::paint(HDC hdc) {
    RECT rc;
    GetClientRect(hwnd_, &rc);

    // Double buffer
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
    SelectObject(memDC, memBmp);

    Gdiplus::Graphics g(memDC);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

    // Dark background
    Gdiplus::SolidBrush bgBrush(Gdiplus::Color(240, 30, 30, 30));
    Gdiplus::RectF fullRect(0, 0, (float)rc.right, (float)rc.bottom);
    g.FillRectangle(&bgBrush, fullRect);

    // Content area (above status bar)
    float contentHeight = (float)(rc.bottom - STATUS_BAR_HEIGHT);
    Gdiplus::RectF contentRect(0, 0, (float)rc.right, contentHeight);

    if (state_ == OverlayState::Recording || state_ == OverlayState::Processing) {
        paintWaveform(g, contentRect);
    }
    // Done state: the EDIT control is shown instead

    // Separator line
    Gdiplus::Pen sepPen(Gdiplus::Color(60, 255, 255, 255));
    g.DrawLine(&sepPen, 0.0f, contentHeight, (float)rc.right, contentHeight);

    // Status bar
    Gdiplus::RectF statusRect(0, contentHeight, (float)rc.right, (float)STATUS_BAR_HEIGHT);
    paintStatusBar(g, statusRect);

    BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);
    DeleteObject(memBmp);
    DeleteDC(memDC);
}

void OverlayWindow::paintWaveform(Gdiplus::Graphics& g, const Gdiplus::RectF& area) {
    float barWidth = 2.5f;
    float spacing = 2.0f;
    int barCount = (int)(area.Width / (barWidth + spacing));
    float startX = area.X + (area.Width - barCount * (barWidth + spacing)) / 2;

    bool frozen = (state_ == OverlayState::Processing);
    Gdiplus::SolidBrush barBrush(Gdiplus::Color(frozen ? 100 : 200, 255, 255, 255));

    for (int i = 0; i < barCount; i++) {
        float level = 0.03f;
        if (!waveformLevels_.empty()) {
            int idx = i * (int)waveformLevels_.size() / barCount;
            if (idx < (int)waveformLevels_.size()) {
                float raw = waveformLevels_[idx];
                level = std::max(0.03f, std::min(1.0f, (float)(log10(1.0 + raw * 100.0) / log10(101.0))));
            }
        }

        float height = std::max(2.0f, level * area.Height * 0.8f);
        float x = startX + i * (barWidth + spacing);
        float y = area.Y + (area.Height - height) / 2;
        g.FillRectangle(&barBrush, x, y, barWidth, height);
    }

    if (state_ == OverlayState::Processing) {
        // Processing indicator
        Gdiplus::Font font(L"Segoe UI", 12);
        Gdiplus::SolidBrush textBrush(Gdiplus::Color(200, 255, 255, 255));
        Gdiplus::StringFormat sf;
        sf.SetAlignment(Gdiplus::StringAlignmentCenter);
        sf.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        g.DrawString(L"Processing...", -1, &font, area, &sf, &textBrush);
    }
}

void OverlayWindow::paintStatusBar(Gdiplus::Graphics& g, const Gdiplus::RectF& area) {
    Gdiplus::Font font(L"Segoe UI", 11);
    Gdiplus::Font smallFont(L"Segoe UI", 9);

    // Status icon + label
    Gdiplus::Color statusColor;
    const wchar_t* statusText;
    switch (state_) {
    case OverlayState::Recording:
        statusColor = Gdiplus::Color(255, 220, 50, 50);
        statusText = L"Recording";
        break;
    case OverlayState::Processing:
        statusColor = Gdiplus::Color(255, 80, 140, 255);
        statusText = L"Processing";
        break;
    case OverlayState::Done:
        statusColor = Gdiplus::Color(255, 50, 200, 80);
        statusText = L"Done";
        break;
    }

    // Status dot
    Gdiplus::SolidBrush dotBrush(statusColor);
    g.FillEllipse(&dotBrush, area.X + 16, area.Y + 10, 10, 10);

    // Status text
    Gdiplus::SolidBrush textBrush(Gdiplus::Color(220, 255, 255, 255));
    Gdiplus::PointF textPos(area.X + 32, area.Y + 6);
    g.DrawString(statusText, -1, &font, textPos, &textBrush);

    // Mode name (center)
    Gdiplus::SolidBrush dimBrush(Gdiplus::Color(140, 255, 255, 255));
    std::wstring wmode = utf8_to_wide(modeName_);
    Gdiplus::StringFormat centerFmt;
    centerFmt.SetAlignment(Gdiplus::StringAlignmentCenter);
    centerFmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
    Gdiplus::RectF modeRect(area.X + 150, area.Y, 150, area.Height);
    g.DrawString(wmode.c_str(), -1, &smallFont, modeRect, &centerFmt, &dimBrush);

    // Shortcuts (right side)
    Gdiplus::RectF shortcutRect(area.X + area.Width - 250, area.Y, 240, area.Height);
    const wchar_t* shortcuts = (state_ == OverlayState::Recording)
        ? L"Stop  Alt+Space  |  Cancel  Esc"
        : L"Record  Alt+Space  |  Close  Esc";
    g.DrawString(shortcuts, -1, &smallFont, shortcutRect, &centerFmt, &dimBrush);
}

} // namespace sasayaku

#endif
