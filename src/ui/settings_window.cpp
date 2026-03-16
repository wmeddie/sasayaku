#include "settings_window.hpp"
#include <iostream>

namespace sasayaku {

SettingsWindow::SettingsWindow() {
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

void SettingsWindow::create_ui() {
    // Create window
    window_ = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(window_), "Sasayaku Settings");
    gtk_window_set_default_size(GTK_WINDOW(window_), 500, 400);
    gtk_window_set_modal(GTK_WINDOW(window_), TRUE);
    g_signal_connect(window_, "close-request", G_CALLBACK(on_close_request), this);

    // Create scrolled window
    GtkWidget* scrolled = gtk_scrolled_window_new();
    gtk_window_set_child(GTK_WINDOW(window_), scrolled);

    // Create main box
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
    gtk_widget_set_margin_start(vbox, 20);
    gtk_widget_set_margin_end(vbox, 20);
    gtk_widget_set_margin_top(vbox, 20);
    gtk_widget_set_margin_bottom(vbox, 20);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), vbox);

    // API Settings Section
    GtkWidget* api_label = gtk_label_new(nullptr);
    gtk_label_set_markup(GTK_LABEL(api_label), "<b>OpenAI API Settings</b>");
    gtk_widget_set_halign(api_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(vbox), api_label);

    // API Base URL
    GtkWidget* base_url_label = gtk_label_new("API Base URL:");
    gtk_widget_set_halign(base_url_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(vbox), base_url_label);

    api_base_url_entry_ = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(api_base_url_entry_), "https://api.openai.com/v1");
    gtk_box_append(GTK_BOX(vbox), api_base_url_entry_);

    GtkWidget* base_url_hint = gtk_label_new("For local llama.cpp: http://localhost:8080/v1");
    gtk_widget_set_halign(base_url_hint, GTK_ALIGN_START);
    gtk_widget_add_css_class(base_url_hint, "dim-label");
    gtk_box_append(GTK_BOX(vbox), base_url_hint);

    // API Key
    GtkWidget* api_key_label = gtk_label_new("API Key:");
    gtk_widget_set_halign(api_key_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(vbox), api_key_label);

    api_key_entry_ = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(api_key_entry_), "sk-...");
    gtk_entry_set_visibility(GTK_ENTRY(api_key_entry_), FALSE);
    gtk_box_append(GTK_BOX(vbox), api_key_entry_);

    GtkWidget* api_key_hint = gtk_label_new("Leave empty if using local llama.cpp");
    gtk_widget_set_halign(api_key_hint, GTK_ALIGN_START);
    gtk_widget_add_css_class(api_key_hint, "dim-label");
    gtk_box_append(GTK_BOX(vbox), api_key_hint);

    // API Model
    GtkWidget* model_label = gtk_label_new("Model:");
    gtk_widget_set_halign(model_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(vbox), model_label);

    api_model_entry_ = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(api_model_entry_), "gpt-4o-mini");
    gtk_box_append(GTK_BOX(vbox), api_model_entry_);

    // Separator
    GtkWidget* separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append(GTK_BOX(vbox), separator);

    // Whisper Settings Section
    GtkWidget* whisper_label = gtk_label_new(nullptr);
    gtk_label_set_markup(GTK_LABEL(whisper_label), "<b>Whisper Settings</b>");
    gtk_widget_set_halign(whisper_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(vbox), whisper_label);

    // Model Path
    GtkWidget* model_path_label = gtk_label_new("Whisper Model Path:");
    gtk_widget_set_halign(model_path_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(vbox), model_path_label);

    GtkWidget* path_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    whisper_model_entry_ = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(whisper_model_entry_), "~/.local/share/sasayaku/models/ggml-large-v3-turbo.bin");
    gtk_widget_set_hexpand(whisper_model_entry_, TRUE);
    gtk_box_append(GTK_BOX(path_hbox), whisper_model_entry_);
    gtk_box_append(GTK_BOX(vbox), path_hbox);

    // GPU Switch
    GtkWidget* gpu_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget* gpu_label = gtk_label_new("Use GPU (CUDA):");
    gtk_widget_set_halign(gpu_label, GTK_ALIGN_START);
    use_gpu_switch_ = gtk_switch_new();
    gtk_switch_set_active(GTK_SWITCH(use_gpu_switch_), TRUE);
    gtk_box_append(GTK_BOX(gpu_hbox), gpu_label);
    gtk_box_append(GTK_BOX(gpu_hbox), use_gpu_switch_);
    gtk_box_append(GTK_BOX(vbox), gpu_hbox);

    // Save button
    GtkWidget* button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(button_box, GTK_ALIGN_END);
    gtk_widget_set_margin_top(button_box, 20);

    save_button_ = gtk_button_new_with_label("Save Settings");
    gtk_widget_add_css_class(save_button_, "suggested-action");
    g_signal_connect(save_button_, "clicked", G_CALLBACK(on_save_clicked), this);
    gtk_box_append(GTK_BOX(button_box), save_button_);

    gtk_box_append(GTK_BOX(vbox), button_box);
}

void SettingsWindow::show() {
    if (window_) {
        gtk_window_present(GTK_WINDOW(window_));
    }
}

void SettingsWindow::hide() {
    if (window_) {
        gtk_widget_set_visible(window_, FALSE);
    }
}

void SettingsWindow::set_settings(const SettingsData& settings) {
    if (api_base_url_entry_) {
        gtk_editable_set_text(GTK_EDITABLE(api_base_url_entry_), settings.api_base_url.c_str());
    }
    if (api_key_entry_) {
        gtk_editable_set_text(GTK_EDITABLE(api_key_entry_), settings.api_key.c_str());
    }
    if (api_model_entry_) {
        gtk_editable_set_text(GTK_EDITABLE(api_model_entry_), settings.api_model.c_str());
    }
    if (whisper_model_entry_) {
        gtk_editable_set_text(GTK_EDITABLE(whisper_model_entry_), settings.whisper_model_path.c_str());
    }
    if (use_gpu_switch_) {
        gtk_switch_set_active(GTK_SWITCH(use_gpu_switch_), settings.use_gpu);
    }
}

SettingsData SettingsWindow::get_current_settings() {
    SettingsData settings;

    if (api_base_url_entry_) {
        settings.api_base_url = gtk_editable_get_text(GTK_EDITABLE(api_base_url_entry_));
    }
    if (api_key_entry_) {
        settings.api_key = gtk_editable_get_text(GTK_EDITABLE(api_key_entry_));
    }
    if (api_model_entry_) {
        settings.api_model = gtk_editable_get_text(GTK_EDITABLE(api_model_entry_));
    }
    if (whisper_model_entry_) {
        settings.whisper_model_path = gtk_editable_get_text(GTK_EDITABLE(whisper_model_entry_));
    }
    if (use_gpu_switch_) {
        settings.use_gpu = gtk_switch_get_active(GTK_SWITCH(use_gpu_switch_));
    }

    return settings;
}

void SettingsWindow::on_save_clicked(GtkButton* button, gpointer user_data) {
    auto* window = static_cast<SettingsWindow*>(user_data);

    SettingsData settings = window->get_current_settings();

    if (window->save_callback_) {
        window->save_callback_(settings);
    }

    // Show confirmation
    std::cout << "Settings saved successfully" << std::endl;

    // Hide window
    window->hide();
}

gboolean SettingsWindow::on_close_request(GtkWindow* gtk_window, gpointer user_data) {
    auto* window = static_cast<SettingsWindow*>(user_data);
    window->hide();
    return TRUE;
}

} // namespace sasayaku
