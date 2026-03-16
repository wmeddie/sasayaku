#include "recording_window.hpp"
#include <iostream>

namespace sasayaku {

RecordingWindow::RecordingWindow() {
}

RecordingWindow::~RecordingWindow() {
    if (window_) {
        gtk_window_destroy(GTK_WINDOW(window_));
    }
}

bool RecordingWindow::initialize(const std::vector<ModeInfo>& modes) {
    modes_ = modes;

    // Initialize GTK if not already done
    gtk_init();

    create_ui();
    return window_ != nullptr;
}

void RecordingWindow::create_ui() {
    // Create main window
    window_ = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(window_), "Sasayaku - Voice Dictation");
    gtk_window_set_default_size(GTK_WINDOW(window_), 500, 400);
    g_signal_connect(window_, "close-request", G_CALLBACK(on_close_request), this);

    // Create main vertical box
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(vbox, 20);
    gtk_widget_set_margin_end(vbox, 20);
    gtk_widget_set_margin_top(vbox, 20);
    gtk_widget_set_margin_bottom(vbox, 20);
    gtk_window_set_child(GTK_WINDOW(window_), vbox);

    // Header box with mode selection and settings button
    GtkWidget* header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_append(GTK_BOX(vbox), header_box);

    // Mode selection section
    GtkWidget* mode_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_hexpand(mode_box, TRUE);
    gtk_box_append(GTK_BOX(header_box), mode_box);

    GtkWidget* mode_label = gtk_label_new("Mode:");
    gtk_widget_set_halign(mode_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(mode_box), mode_label);

    // Create string list for dropdown
    GtkStringList* string_list = gtk_string_list_new(nullptr);
    for (const auto& mode : modes_) {
        gtk_string_list_append(string_list, mode.name.c_str());
    }

    mode_dropdown_ = gtk_drop_down_new(G_LIST_MODEL(string_list), nullptr);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(mode_dropdown_), 0);
    g_signal_connect(mode_dropdown_, "notify::selected", G_CALLBACK(on_mode_changed), this);
    gtk_box_append(GTK_BOX(mode_box), mode_dropdown_);

    // Settings button
    settings_button_ = gtk_button_new_with_label("⚙️  Settings");
    gtk_widget_set_valign(settings_button_, GTK_ALIGN_END);
    g_signal_connect(settings_button_, "clicked", G_CALLBACK(on_settings_button_clicked), this);
    gtk_box_append(GTK_BOX(header_box), settings_button_);

    // Status label
    status_label_ = gtk_label_new("Ready to record");
    gtk_widget_set_halign(status_label_, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(vbox), status_label_);

    // Audio level bar
    GtkWidget* level_label = gtk_label_new("Audio Level:");
    gtk_widget_set_halign(level_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(vbox), level_label);

    audio_level_bar_ = gtk_level_bar_new();
    gtk_level_bar_set_min_value(GTK_LEVEL_BAR(audio_level_bar_), 0.0);
    gtk_level_bar_set_max_value(GTK_LEVEL_BAR(audio_level_bar_), 1.0);
    gtk_level_bar_set_value(GTK_LEVEL_BAR(audio_level_bar_), 0.0);
    gtk_box_append(GTK_BOX(vbox), audio_level_bar_);

    // Record button
    record_button_ = gtk_button_new_with_label("🎤 Start Recording");
    gtk_widget_set_size_request(record_button_, -1, 50);
    g_signal_connect(record_button_, "clicked", G_CALLBACK(on_record_button_clicked), this);
    gtk_box_append(GTK_BOX(vbox), record_button_);

    // Transcription text view
    GtkWidget* text_label = gtk_label_new("Transcription:");
    gtk_widget_set_halign(text_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(vbox), text_label);

    GtkWidget* scrolled = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scrolled), 150);

    transcription_text_view_ = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(transcription_text_view_), GTK_WRAP_WORD);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(transcription_text_view_), FALSE);
    text_buffer_ = gtk_text_view_get_buffer(GTK_TEXT_VIEW(transcription_text_view_));

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), transcription_text_view_);
    gtk_box_append(GTK_BOX(vbox), scrolled);
}

void RecordingWindow::show() {
    if (window_) {
        gtk_window_present(GTK_WINDOW(window_));
    }
}

void RecordingWindow::hide() {
    if (window_) {
        gtk_widget_set_visible(window_, FALSE);
    }
}

void RecordingWindow::set_recording_state(bool recording) {
    is_recording_ = recording;

    if (record_button_) {
        if (recording) {
            gtk_button_set_label(GTK_BUTTON(record_button_), "⏹️  Stop Recording");
        } else {
            gtk_button_set_label(GTK_BUTTON(record_button_), "🎤 Start Recording");
        }
    }

    if (!recording) {
        // Reset audio level when stopped
        set_audio_level(0.0);
    }
}

void RecordingWindow::set_status_text(const std::string& text) {
    if (status_label_) {
        gtk_label_set_text(GTK_LABEL(status_label_), text.c_str());
    }
}

void RecordingWindow::set_audio_level(float level) {
    if (audio_level_bar_) {
        gtk_level_bar_set_value(GTK_LEVEL_BAR(audio_level_bar_), level);
    }
}

void RecordingWindow::set_transcription_text(const std::string& text) {
    if (text_buffer_) {
        gtk_text_buffer_set_text(text_buffer_, text.c_str(), -1);
    }
}

std::string RecordingWindow::get_selected_mode() const {
    if (!mode_dropdown_ || modes_.empty()) {
        return "";
    }

    guint selected = gtk_drop_down_get_selected(GTK_DROP_DOWN(mode_dropdown_));
    if (selected < modes_.size()) {
        return modes_[selected].id;
    }

    return "";
}

void RecordingWindow::on_record_button_clicked(GtkButton* button, gpointer user_data) {
    auto* window = static_cast<RecordingWindow*>(user_data);

    if (window->record_callback_) {
        window->record_callback_(!window->is_recording_);
    }
}

void RecordingWindow::on_mode_changed(GtkDropDown* dropdown, GParamSpec* pspec, gpointer user_data) {
    auto* window = static_cast<RecordingWindow*>(user_data);

    if (window->mode_changed_callback_) {
        std::string mode = window->get_selected_mode();
        if (!mode.empty()) {
            window->mode_changed_callback_(mode);
        }
    }
}

void RecordingWindow::on_settings_button_clicked(GtkButton* button, gpointer user_data) {
    auto* window = static_cast<RecordingWindow*>(user_data);

    if (window->settings_callback_) {
        window->settings_callback_();
    }
}

gboolean RecordingWindow::on_close_request(GtkWindow* gtk_window, gpointer user_data) {
    auto* window = static_cast<RecordingWindow*>(user_data);

    std::cout << "Window close requested - hiding window" << std::endl;

    // Just hide instead of destroying
    gtk_widget_set_visible(GTK_WIDGET(gtk_window), FALSE);

    // Return TRUE to prevent default close behavior and app termination
    return TRUE;
}

} // namespace sasayaku
