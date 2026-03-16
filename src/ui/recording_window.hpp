#pragma once

#include <gtk/gtk.h>
#include <string>
#include <functional>
#include <vector>

namespace sasayaku {

struct ModeInfo {
    std::string id;
    std::string name;
    std::string description;
};

using RecordButtonCallback = std::function<void(bool is_recording)>;
using ModeChangedCallback = std::function<void(const std::string& mode_id)>;
using SettingsButtonCallback = std::function<void()>;

class RecordingWindow {
public:
    RecordingWindow();
    ~RecordingWindow();

    // Initialize the window
    bool initialize(const std::vector<ModeInfo>& modes);

    // Show/hide window
    void show();
    void hide();

    // Set callbacks
    void set_record_callback(RecordButtonCallback cb) { record_callback_ = cb; }
    void set_mode_changed_callback(ModeChangedCallback cb) { mode_changed_callback_ = cb; }
    void set_settings_callback(SettingsButtonCallback cb) { settings_callback_ = cb; }

    // Update UI state
    void set_recording_state(bool recording);
    void set_status_text(const std::string& text);
    void set_audio_level(float level);  // 0.0 to 1.0
    void set_transcription_text(const std::string& text);

    // Get selected mode
    std::string get_selected_mode() const;

private:
    GtkWidget* window_ = nullptr;
    GtkWidget* record_button_ = nullptr;
    GtkWidget* settings_button_ = nullptr;
    GtkWidget* mode_dropdown_ = nullptr;
    GtkWidget* status_label_ = nullptr;
    GtkWidget* audio_level_bar_ = nullptr;
    GtkWidget* transcription_text_view_ = nullptr;
    GtkTextBuffer* text_buffer_ = nullptr;

    bool is_recording_ = false;
    std::vector<ModeInfo> modes_;

    RecordButtonCallback record_callback_;
    ModeChangedCallback mode_changed_callback_;
    SettingsButtonCallback settings_callback_;

    static void on_record_button_clicked(GtkButton* button, gpointer user_data);
    static void on_settings_button_clicked(GtkButton* button, gpointer user_data);
    static void on_mode_changed(GtkDropDown* dropdown, GParamSpec* pspec, gpointer user_data);
    static gboolean on_close_request(GtkWindow* window, gpointer user_data);

    void create_ui();
};

} // namespace sasayaku
