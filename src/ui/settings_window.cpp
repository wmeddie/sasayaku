#include "settings_window.hpp"
#include <iostream>
#include <filesystem>
#include <thread>
#include <curl/curl.h>

namespace sasayaku {

static const std::vector<std::pair<std::string, std::string>> MODEL_LIST = {
    {"ggml-tiny.bin",             "Tiny (75 MB, fastest)"},
    {"ggml-base.bin",             "Base (142 MB)"},
    {"ggml-small.bin",            "Small (466 MB)"},
    {"ggml-medium.bin",           "Medium (1.5 GB)"},
    {"ggml-large-v3-turbo.bin",   "Large v3 Turbo (1.6 GB)"},
    {"ggml-large-v3.bin",         "Large v3 (3.1 GB, most accurate)"},
};

SettingsWindow::SettingsWindow() {
    for (const auto& [filename, label] : MODEL_LIST) {
        available_models_.push_back({filename, label, "", false});
    }
}

SettingsWindow::~SettingsWindow() {
    if (window_) {
        gtk_window_destroy(GTK_WINDOW(window_));
    }
}

bool SettingsWindow::initialize() {
    create_ui();
    return window_ != nullptr;
}

void SettingsWindow::scan_downloaded_models() {
    for (auto& model : available_models_) {
        std::string path = models_dir_ + "/" + model.filename;
        model.downloaded = std::filesystem::exists(path);
    }
}

void SettingsWindow::create_ui() {
    window_ = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(window_), "Sasayaku Settings");
    gtk_window_set_default_size(GTK_WINDOW(window_), 550, 550);
    gtk_window_set_modal(GTK_WINDOW(window_), TRUE);
    g_signal_connect(window_, "close-request", G_CALLBACK(on_close_request), this);

    GtkWidget* scrolled = gtk_scrolled_window_new();
    gtk_window_set_child(GTK_WINDOW(window_), scrolled);

    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(vbox, 20);
    gtk_widget_set_margin_end(vbox, 20);
    gtk_widget_set_margin_top(vbox, 20);
    gtk_widget_set_margin_bottom(vbox, 20);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), vbox);

    // --- AI API Section ---
    GtkWidget* api_label = gtk_label_new(nullptr);
    gtk_label_set_markup(GTK_LABEL(api_label), "<b>AI API (Ollama / OpenAI-compatible)</b>");
    gtk_widget_set_halign(api_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(vbox), api_label);

    GtkWidget* base_url_label = gtk_label_new("API Base URL:");
    gtk_widget_set_halign(base_url_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(vbox), base_url_label);

    api_base_url_entry_ = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(api_base_url_entry_), "http://localhost:11434/v1/");
    gtk_box_append(GTK_BOX(vbox), api_base_url_entry_);

    GtkWidget* base_url_hint = gtk_label_new("Ollama: http://localhost:11434/v1/");
    gtk_widget_set_halign(base_url_hint, GTK_ALIGN_START);
    gtk_widget_add_css_class(base_url_hint, "dim-label");
    gtk_box_append(GTK_BOX(vbox), base_url_hint);

    GtkWidget* api_key_label = gtk_label_new("API Key:");
    gtk_widget_set_halign(api_key_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(vbox), api_key_label);

    api_key_entry_ = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(api_key_entry_), "ollama");
    gtk_entry_set_visibility(GTK_ENTRY(api_key_entry_), FALSE);
    gtk_box_append(GTK_BOX(vbox), api_key_entry_);

    GtkWidget* model_label = gtk_label_new("LLM Model:");
    gtk_widget_set_halign(model_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(vbox), model_label);

    api_model_entry_ = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(api_model_entry_), "qwen3:0.6b");
    gtk_box_append(GTK_BOX(vbox), api_model_entry_);

    // --- Separator ---
    gtk_box_append(GTK_BOX(vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

    // --- Whisper Model Section ---
    GtkWidget* whisper_label = gtk_label_new(nullptr);
    gtk_label_set_markup(GTK_LABEL(whisper_label), "<b>Whisper Model</b>");
    gtk_widget_set_halign(whisper_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(vbox), whisper_label);

    // Model dropdown
    GtkWidget* dropdown_label = gtk_label_new("Select model:");
    gtk_widget_set_halign(dropdown_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(vbox), dropdown_label);

    // Build string list for dropdown
    GtkStringList* string_list = gtk_string_list_new(nullptr);
    for (const auto& model : available_models_) {
        gtk_string_list_append(string_list, model.label.c_str());
    }

    model_dropdown_ = gtk_drop_down_new(G_LIST_MODEL(string_list), nullptr);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(model_dropdown_), 0);
    g_signal_connect(model_dropdown_, "notify::selected", G_CALLBACK(on_model_changed), this);
    gtk_box_append(GTK_BOX(vbox), model_dropdown_);

    // Download row
    GtkWidget* download_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_top(download_hbox, 4);

    download_status_label_ = gtk_label_new("");
    gtk_widget_set_halign(download_status_label_, GTK_ALIGN_START);
    gtk_widget_set_hexpand(download_status_label_, TRUE);
    gtk_box_append(GTK_BOX(download_hbox), download_status_label_);

    download_button_ = gtk_button_new_with_label("Download");
    g_signal_connect(download_button_, "clicked", G_CALLBACK(on_download_clicked), this);
    gtk_box_append(GTK_BOX(download_hbox), download_button_);

    gtk_box_append(GTK_BOX(vbox), download_hbox);

    // Progress bar
    download_progress_ = gtk_progress_bar_new();
    gtk_widget_set_visible(download_progress_, FALSE);
    gtk_box_append(GTK_BOX(vbox), download_progress_);

    // Model path (hidden, auto-populated)
    whisper_model_entry_ = gtk_entry_new();
    gtk_widget_set_visible(whisper_model_entry_, FALSE);
    gtk_box_append(GTK_BOX(vbox), whisper_model_entry_);

    GtkWidget* models_hint = gtk_label_new("Models from huggingface.co/ggerganov/whisper.cpp");
    gtk_widget_set_halign(models_hint, GTK_ALIGN_START);
    gtk_widget_add_css_class(models_hint, "dim-label");
    gtk_box_append(GTK_BOX(vbox), models_hint);

    // --- Separator ---
    gtk_box_append(GTK_BOX(vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

    // GPU Switch
    GtkWidget* gpu_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget* gpu_label = gtk_label_new("Use GPU (CUDA):");
    gtk_widget_set_halign(gpu_label, GTK_ALIGN_START);
    gtk_widget_set_hexpand(gpu_label, TRUE);
    use_gpu_switch_ = gtk_switch_new();
    gtk_switch_set_active(GTK_SWITCH(use_gpu_switch_), TRUE);
    gtk_box_append(GTK_BOX(gpu_hbox), gpu_label);
    gtk_box_append(GTK_BOX(gpu_hbox), use_gpu_switch_);
    gtk_box_append(GTK_BOX(vbox), gpu_hbox);

    // Save button
    GtkWidget* button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(button_box, GTK_ALIGN_END);
    gtk_widget_set_margin_top(button_box, 15);

    save_button_ = gtk_button_new_with_label("Save Settings");
    gtk_widget_add_css_class(save_button_, "suggested-action");
    g_signal_connect(save_button_, "clicked", G_CALLBACK(on_save_clicked), this);
    gtk_box_append(GTK_BOX(button_box), save_button_);

    gtk_box_append(GTK_BOX(vbox), button_box);
}

void SettingsWindow::show() {
    if (window_) {
        scan_downloaded_models();
        on_model_selected();
        gtk_window_present(GTK_WINDOW(window_));
    }
}

void SettingsWindow::hide() {
    if (window_) {
        gtk_widget_set_visible(window_, FALSE);
    }
}

void SettingsWindow::set_settings(const SettingsData& settings) {
    if (api_base_url_entry_)
        gtk_editable_set_text(GTK_EDITABLE(api_base_url_entry_), settings.api_base_url.c_str());
    if (api_key_entry_)
        gtk_editable_set_text(GTK_EDITABLE(api_key_entry_), settings.api_key.c_str());
    if (api_model_entry_)
        gtk_editable_set_text(GTK_EDITABLE(api_model_entry_), settings.api_model.c_str());
    if (whisper_model_entry_)
        gtk_editable_set_text(GTK_EDITABLE(whisper_model_entry_), settings.whisper_model_path.c_str());
    if (use_gpu_switch_)
        gtk_switch_set_active(GTK_SWITCH(use_gpu_switch_), settings.use_gpu);

    // Select the matching model in the dropdown
    std::string current_path = settings.whisper_model_path;
    for (int i = 0; i < (int)available_models_.size(); i++) {
        if (current_path.find(available_models_[i].filename) != std::string::npos) {
            gtk_drop_down_set_selected(GTK_DROP_DOWN(model_dropdown_), i);
            selected_model_index_ = i;
            break;
        }
    }
}

SettingsData SettingsWindow::get_current_settings() {
    SettingsData settings;
    if (api_base_url_entry_)
        settings.api_base_url = gtk_editable_get_text(GTK_EDITABLE(api_base_url_entry_));
    if (api_key_entry_)
        settings.api_key = gtk_editable_get_text(GTK_EDITABLE(api_key_entry_));
    if (api_model_entry_)
        settings.api_model = gtk_editable_get_text(GTK_EDITABLE(api_model_entry_));
    if (use_gpu_switch_)
        settings.use_gpu = gtk_switch_get_active(GTK_SWITCH(use_gpu_switch_));

    // Build model path from selected model
    if (selected_model_index_ >= 0 && selected_model_index_ < (int)available_models_.size()) {
        settings.whisper_model_path = models_dir_ + "/" + available_models_[selected_model_index_].filename;
    } else if (whisper_model_entry_) {
        settings.whisper_model_path = gtk_editable_get_text(GTK_EDITABLE(whisper_model_entry_));
    }

    return settings;
}

void SettingsWindow::update_download_button() {
    if (selected_model_index_ < 0 || selected_model_index_ >= (int)available_models_.size()) return;

    const auto& model = available_models_[selected_model_index_];
    if (model.downloaded) {
        gtk_label_set_text(GTK_LABEL(download_status_label_), "Downloaded");
        gtk_widget_add_css_class(download_status_label_, "success");
        gtk_widget_set_sensitive(download_button_, FALSE);
        gtk_button_set_label(GTK_BUTTON(download_button_), "Downloaded");
    } else {
        gtk_label_set_text(GTK_LABEL(download_status_label_), "Not downloaded");
        gtk_widget_remove_css_class(download_status_label_, "success");
        gtk_widget_set_sensitive(download_button_, TRUE);
        gtk_button_set_label(GTK_BUTTON(download_button_), "Download");
    }
}

void SettingsWindow::on_model_selected() {
    selected_model_index_ = gtk_drop_down_get_selected(GTK_DROP_DOWN(model_dropdown_));
    update_download_button();
}

// curl progress callback
struct DownloadContext {
    SettingsWindow* window;
    std::string dest_path;
    std::string temp_path;
    std::string filename;
};

static size_t write_file_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    FILE* fp = static_cast<FILE*>(userp);
    return fwrite(contents, size, nmemb, fp);
}

static int download_progress_callback(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
                                       curl_off_t /*ultotal*/, curl_off_t /*ulnow*/) {
    auto* ctx = static_cast<DownloadContext*>(clientp);
    if (dltotal > 0) {
        double fraction = static_cast<double>(dlnow) / static_cast<double>(dltotal);
        // Update progress on main thread
        g_idle_add([](gpointer data) -> gboolean {
            auto* pair = static_cast<std::pair<GtkWidget*, double>*>(data);
            gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(pair->first), pair->second);
            delete pair;
            return G_SOURCE_REMOVE;
        }, new std::pair<GtkWidget*, double>(ctx->window->download_progress_bar(), fraction));
    }
    return 0;
}

void SettingsWindow::download_selected_model() {
    if (selected_model_index_ < 0 || selected_model_index_ >= (int)available_models_.size()) return;

    const auto& model = available_models_[selected_model_index_];
    std::string url = "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/" + model.filename;

    // Create models directory
    std::filesystem::create_directories(models_dir_);

    std::string dest_path = models_dir_ + "/" + model.filename;
    std::string temp_path = dest_path + ".tmp";

    // Update UI
    gtk_widget_set_visible(download_progress_, TRUE);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(download_progress_), 0.0);
    gtk_widget_set_sensitive(download_button_, FALSE);
    gtk_button_set_label(GTK_BUTTON(download_button_), "Downloading...");
    gtk_label_set_text(GTK_LABEL(download_status_label_), "Downloading...");

    // Download in background thread
    auto* ctx = new DownloadContext{this, dest_path, temp_path, model.filename};
    int model_idx = selected_model_index_;

    std::thread([this, ctx, url, model_idx]() {
        FILE* fp = fopen(ctx->temp_path.c_str(), "wb");
        bool success = false;

        if (fp) {
            CURL* curl = curl_easy_init();
            if (curl) {
                curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file_callback);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
                curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
                curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, download_progress_callback);
                curl_easy_setopt(curl, CURLOPT_XFERINFODATA, ctx);
                curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

                CURLcode res = curl_easy_perform(curl);
                success = (res == CURLE_OK);
                curl_easy_cleanup(curl);
            }
            fclose(fp);
        }

        if (success) {
            std::filesystem::rename(ctx->temp_path, ctx->dest_path);
        } else {
            std::filesystem::remove(ctx->temp_path);
        }

        // Update UI on main thread
        bool final_success = success;
        g_idle_add([](gpointer data) -> gboolean {
            auto* pair = static_cast<std::pair<SettingsWindow*, bool>*>(data);
            auto* self = pair->first;
            bool ok = pair->second;
            delete pair;

            gtk_widget_set_visible(self->download_progress_, FALSE);
            if (ok) {
                self->scan_downloaded_models();
                self->update_download_button();
                gtk_label_set_text(GTK_LABEL(self->download_status_label_), "Download complete");
            } else {
                gtk_widget_set_sensitive(self->download_button_, TRUE);
                gtk_button_set_label(GTK_BUTTON(self->download_button_), "Retry");
                gtk_label_set_text(GTK_LABEL(self->download_status_label_), "Download failed");
            }
            return G_SOURCE_REMOVE;
        }, new std::pair<SettingsWindow*, bool>(this, final_success));

        delete ctx;
    }).detach();
}

void SettingsWindow::on_save_clicked(GtkButton* /*button*/, gpointer user_data) {
    auto* window = static_cast<SettingsWindow*>(user_data);
    SettingsData settings = window->get_current_settings();

    if (window->save_callback_) {
        window->save_callback_(settings);
    }

    std::cout << "Settings saved" << std::endl;
    window->hide();
}

void SettingsWindow::on_download_clicked(GtkButton* /*button*/, gpointer user_data) {
    auto* window = static_cast<SettingsWindow*>(user_data);
    window->download_selected_model();
}

void SettingsWindow::on_model_changed(GObject* /*dropdown*/, GParamSpec* /*pspec*/, gpointer user_data) {
    auto* window = static_cast<SettingsWindow*>(user_data);
    window->on_model_selected();
}

gboolean SettingsWindow::on_close_request(GtkWindow* /*gtk_window*/, gpointer user_data) {
    auto* window = static_cast<SettingsWindow*>(user_data);
    window->hide();
    return TRUE;
}

} // namespace sasayaku
