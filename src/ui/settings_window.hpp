#pragma once

#include <gtk/gtk.h>
#include <string>
#include <functional>
#include <vector>

namespace sasayaku {

struct SettingsData {
    std::string api_base_url;
    std::string api_key;
    std::string api_model;
    std::string whisper_model_path;
    bool use_gpu;
};

struct WhisperModelInfo {
    std::string filename;
    std::string label;
    std::string size;
    bool downloaded;
};

using SettingsSaveCallback = std::function<void(const SettingsData& settings)>;

class SettingsWindow {
public:
    SettingsWindow();
    ~SettingsWindow();

    bool initialize();
    void show();
    void hide();
    void set_settings(const SettingsData& settings);
    void set_save_callback(SettingsSaveCallback cb) { save_callback_ = cb; }

    // Set models directory for download/scan
    void set_models_dir(const std::string& dir) { models_dir_ = dir; }

    // Progress bar widget, accessed by the curl download progress callback
    GtkWidget* download_progress_bar() const { return download_progress_; }

private:
    GtkWidget* window_ = nullptr;
    GtkWidget* api_base_url_entry_ = nullptr;
    GtkWidget* api_key_entry_ = nullptr;
    GtkWidget* api_model_entry_ = nullptr;
    GtkWidget* whisper_model_entry_ = nullptr;
    GtkWidget* use_gpu_switch_ = nullptr;
    GtkWidget* save_button_ = nullptr;
    GtkWidget* model_dropdown_ = nullptr;
    GtkWidget* download_button_ = nullptr;
    GtkWidget* download_progress_ = nullptr;
    GtkWidget* download_status_label_ = nullptr;

    SettingsSaveCallback save_callback_;
    std::string models_dir_;
    std::vector<WhisperModelInfo> available_models_;
    int selected_model_index_ = 0;

    void create_ui();
    SettingsData get_current_settings();
    void scan_downloaded_models();
    void update_download_button();
    void download_selected_model();
    void on_model_selected();

    static void on_save_clicked(GtkButton* button, gpointer user_data);
    static void on_download_clicked(GtkButton* button, gpointer user_data);
    static void on_model_changed(GObject* dropdown, GParamSpec* pspec, gpointer user_data);
    static gboolean on_close_request(GtkWindow* window, gpointer user_data);
};

} // namespace sasayaku
