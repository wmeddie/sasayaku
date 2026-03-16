#include "clipboard.hpp"
#include <gtk/gtk.h>
#include <iostream>

namespace sasayaku {

std::string Clipboard::get_text() {
    GdkDisplay* display = gdk_display_get_default();
    if (!display) {
        return "";
    }

    GdkClipboard* clipboard = gdk_display_get_clipboard(display);
    if (!clipboard) {
        return "";
    }

    // Use simple synchronous blocking call
    // This is acceptable for our use case as we're not in the main UI thread
    GMainContext* context = g_main_context_new();
    g_main_context_push_thread_default(context);

    GMainLoop* loop = g_main_loop_new(context, FALSE);
    std::string result;

    gdk_clipboard_read_text_async(clipboard, nullptr,
        [](GObject* source, GAsyncResult* res, gpointer data) {
            auto* result_ptr = static_cast<std::string*>(data);
            GError* error = nullptr;
            char* text = gdk_clipboard_read_text_finish(GDK_CLIPBOARD(source), res, &error);

            if (text && !error) {
                *result_ptr = text;
                g_free(text);
            }

            if (error) {
                g_error_free(error);
            }

            // Get loop from context
            GMainLoop* loop = static_cast<GMainLoop*>(g_object_get_data(G_OBJECT(source), "loop"));
            if (loop) {
                g_main_loop_quit(loop);
            }
        },
        &result
    );

    g_object_set_data(G_OBJECT(clipboard), "loop", loop);

    // Run loop with timeout
    GSource* timeout = g_timeout_source_new(1000);  // 1 second timeout
    g_source_set_callback(timeout, [](gpointer data) -> gboolean {
        g_main_loop_quit(static_cast<GMainLoop*>(data));
        return G_SOURCE_REMOVE;
    }, loop, nullptr);
    g_source_attach(timeout, context);

    g_main_loop_run(loop);

    g_source_destroy(timeout);
    g_source_unref(timeout);
    g_main_loop_unref(loop);
    g_main_context_pop_thread_default(context);
    g_main_context_unref(context);

    return result;
}

bool Clipboard::set_text(const std::string& text) {
    GdkDisplay* display = gdk_display_get_default();
    if (!display) {
        return false;
    }

    GdkClipboard* clipboard = gdk_display_get_clipboard(display);
    if (!clipboard) {
        return false;
    }

    gdk_clipboard_set_text(clipboard, text.c_str());
    return true;
}

bool Clipboard::has_text() {
    GdkDisplay* display = gdk_display_get_default();
    if (!display) {
        return false;
    }

    GdkClipboard* clipboard = gdk_display_get_clipboard(display);
    if (!clipboard) {
        return false;
    }

    GdkContentFormats* formats = gdk_clipboard_get_formats(clipboard);
    return gdk_content_formats_contain_gtype(formats, G_TYPE_STRING);
}

void Clipboard::clear() {
    set_text("");
}

} // namespace sasayaku
