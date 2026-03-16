#pragma once

#include <gtk/gtk.h>
#include <string>
#include <functional>

namespace sasayaku {

struct SettingsData {
    std::string api_base_url;
    std::string api_key;
    std::string api_model;
    std::string whisper_model_path;
    bool use_gpu;
};

using SettingsSaveCallback = std::function<void(const SettingsData& settings)>;

class SettingsWindow {
public:
    SettingsWindow();
    ~SettingsWindow();

    // Initialize the window
    bool initialize();

    // Show/hide
    void show();
    void hide();

    // Set current settings
    void set_settings(const SettingsData& settings);

    // Set callback for when settings are saved
    void set_save_callback(SettingsSaveCallback cb) { save_callback_ = cb; }

private:
    GtkWidget* window_ = nullptr;
    GtkWidget* api_base_url_entry_ = nullptr;
    GtkWidget* api_key_entry_ = nullptr;
    GtkWidget* api_model_entry_ = nullptr;
    GtkWidget* whisper_model_entry_ = nullptr;
    GtkWidget* use_gpu_switch_ = nullptr;
    GtkWidget* save_button_ = nullptr;

    SettingsSaveCallback save_callback_;

    static void on_save_clicked(GtkButton* button, gpointer user_data);
    static gboolean on_close_request(GtkWindow* window, gpointer user_data);

    void create_ui();
    SettingsData get_current_settings();
};

} // namespace sasayaku
