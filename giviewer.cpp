#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <vector>
#include <string>
#include <dirent.h>
#include <iostream>

class GIViewer {
public:
    GIViewer();
    void run();

private:
    static void on_next_clicked(GtkWidget *widget, gpointer user_data);
    static void on_prev_clicked(GtkWidget *widget, gpointer user_data);
    static void on_open_folder_clicked(GtkWidget *widget, gpointer user_data);
    static gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data);
    static gboolean on_scroll(GtkWidget *widget, GdkEventScroll *event, gpointer user_data);
    static gboolean on_button_press(GtkWidget *widget, GdkEventButton *event, gpointer user_data);
    static gboolean on_button_release(GtkWidget *widget, GdkEventButton *event, gpointer user_data);
    static gboolean on_motion_notify(GtkWidget *widget, GdkEventMotion *event, gpointer user_data);
    static gboolean on_draw(GtkWidget *widget, cairo_t *cr, gpointer user_data);

    GtkWidget *window;
    GtkWidget *scrolled_window;
    GtkWidget *image;
    GtkWidget *prev_button;
    GtkWidget *next_button;
    GtkWidget *open_folder_button;
    std::vector<std::string> image_files;
    int current_image_index;
    double zoom_factor;

    // Cropping variables
    bool is_cropping;
    GdkRectangle crop_area;

    void load_images(const std::string &directory);
    void update_image();
    void resize_image(GdkPixbuf *pixbuf);
    void start_cropping(int x, int y);
    void update_crop_area(int x, int y);
    void finish_cropping();
};

GIViewer::GIViewer() : current_image_index(0), zoom_factor(1.0), is_cropping(false) {
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "GIViewer - GTG's Image Viewer");
    gtk_window_set_icon_from_file(GTK_WINDOW(window), "giviewer.png", NULL);
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(window, "key-press-event", G_CALLBACK(on_key_press), this);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    // Create a scrolled window to contain the image
    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled_window, TRUE, TRUE, 0);

    image = gtk_image_new();
    gtk_container_add(GTK_CONTAINER(scrolled_window), image);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    prev_button = gtk_button_new_with_label("<");
    gtk_box_pack_start(GTK_BOX(hbox), prev_button, TRUE, TRUE, 0);
    g_signal_connect(prev_button, "clicked", G_CALLBACK(on_prev_clicked), this);

    next_button = gtk_button_new_with_label(">");
    gtk_box_pack_start(GTK_BOX(hbox), next_button, TRUE, TRUE, 0);
    g_signal_connect(next_button, "clicked", G_CALLBACK(on_next_clicked), this);

    open_folder_button = gtk_button_new_with_label("Open Folder");
    gtk_box_pack_start(GTK_BOX(hbox), open_folder_button, TRUE, TRUE, 0);
    g_signal_connect(open_folder_button, "clicked", G_CALLBACK(on_open_folder_clicked), this);

    // Set modern UI theme using CSS
    GtkCssProvider *css_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css_provider,
                                    "window { background-color: lightgrey; }"
                                    "button { background-color: lime; color: black; border-radius: 5px; padding: 10px; font-size: 14px; }"
                                    "button:hover { background-color: green; }"
                                    "button:pressed { background-color: darkgreen; }",
                                    -1, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
                                              GTK_STYLE_PROVIDER(css_provider), GTK_STYLE_PROVIDER_PRIORITY_USER);
    g_object_unref(css_provider);

    // Connect mouse events for cropping
    g_signal_connect(window, "button-press-event", G_CALLBACK(on_button_press), this);
    g_signal_connect(window, "button-release-event", G_CALLBACK(on_button_release), this);
    g_signal_connect(window, "motion-notify-event", G_CALLBACK(on_motion_notify), this);
    g_signal_connect(window, "draw", G_CALLBACK(on_draw), this);

    gtk_widget_show_all(window);
}

void GIViewer::load_images(const std::string &directory) {
    DIR *dir;
    struct dirent *ent;

    image_files.clear();  // Clear previous files
    current_image_index = 0;  // Reset index

    if ((dir = opendir(directory.c_str())) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            std::string filename = ent->d_name;

            // Check if the filename is long enough before comparing
            if (filename.size() >= 4 && filename.compare(filename.size() - 4, 4, ".jpg") == 0) {
                image_files.push_back(directory + "/" + filename);
            } else if (filename.size() >= 4 && filename.compare(filename.size() - 4, 4, ".png") == 0) {
                image_files.push_back(directory + "/" + filename);
            } else if (filename.size() >= 5 && filename.compare(filename.size() - 5, 5, ".jpeg") == 0) {
                image_files.push_back(directory + "/" + filename);
            }
        }
        closedir(dir);
    } else {
        std::cerr << "Could not open directory: " << directory << std::endl;
    }

    if (!image_files.empty()) {
        update_image();
    } else {
        gtk_image_set_from_pixbuf(GTK_IMAGE(image), NULL);
    }
}

void GIViewer::resize_image(GdkPixbuf *pixbuf) {
    if (pixbuf) {
        // Get original dimensions
        int original_width = gdk_pixbuf_get_width(pixbuf);
        int original_height = gdk_pixbuf_get_height(pixbuf);

        // Calculate new dimensions based on zoom factor
        int new_height = static_cast<int>(500 * zoom_factor);
        int new_width = static_cast<int>(original_width * (new_height / static_cast<double>(original_height)));

        // Resize the image
        GdkPixbuf *resized_pixbuf = gdk_pixbuf_scale_simple(pixbuf, new_width, new_height, GDK_INTERP_BILINEAR);
        gtk_image_set_from_pixbuf(GTK_IMAGE(image), resized_pixbuf);
        g_object_unref(resized_pixbuf);
    }
}

void GIViewer::update_image() {
    if (!image_files.empty()) {
        GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(image_files[current_image_index].c_str(), NULL);
        resize_image(pixbuf);
        g_object_unref(pixbuf);
    }
}

void GIViewer::start_cropping(int x, int y) {
    is_cropping = true;
    crop_area.x = x - 100;  // Center the circle
    crop_area.y = y - 100;  // Center the circle
    crop_area.width = 200;   // Diameter of the circle
    crop_area.height = 200;  // Diameter of the circle
}

void GIViewer::update_crop_area(int x, int y) {
    if (is_cropping) {
        crop_area.x = x - 100;  // Update center of the circle based on mouse position
        crop_area.y = y - 100;  // Update center of the circle based on mouse position
        gtk_widget_queue_draw(window);  // Request a redraw to show the updated crop area
    }
}

gboolean GIViewer::on_button_press(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    GIViewer *viewer = static_cast<GIViewer*>(user_data);
    if (event->button == GDK_BUTTON_PRIMARY && (event->state & GDK_CONTROL_MASK)) {
        viewer->start_cropping(static_cast<int>(event->x), static_cast<int>(event->y));
    }
    return TRUE;
}

gboolean GIViewer::on_button_release(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    GIViewer *viewer = static_cast<GIViewer*>(user_data);
    if (event->button == GDK_BUTTON_PRIMARY && viewer->is_cropping) {
        viewer->finish_cropping();
    }
    return TRUE;
}

void GIViewer::finish_cropping() {
    is_cropping = false;
    gtk_widget_queue_draw(window);  // Request a redraw to remove the crop area
}

gboolean GIViewer::on_motion_notify(GtkWidget *widget, GdkEventMotion *event, gpointer user_data) {
    GIViewer *viewer = static_cast<GIViewer*>(user_data);
    if (viewer->is_cropping) {
        viewer->update_crop_area(static_cast<int>(event->x), static_cast<int>(event->y));
    }
    return TRUE;
}

gboolean GIViewer::on_draw(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
    GIViewer *viewer = static_cast<GIViewer*>(user_data);
    if (viewer->is_cropping) {
        // Draw the circular cropping area
        cairo_set_source_rgba(cr, 1, 0, 0, 0.5);  // Red color with 0.5 opacity
        cairo_arc(cr, viewer->crop_area.x + 100, viewer->crop_area.y + 100, 100, 0, 2 * G_PI);  // Draw a circle
        cairo_fill(cr);
    }
    return FALSE;
}

void GIViewer::on_next_clicked(GtkWidget *widget, gpointer user_data) {
    GIViewer *viewer = static_cast<GIViewer*>(user_data);
    viewer->current_image_index = (viewer->current_image_index + 1) % viewer->image_files.size();
    viewer->update_image();
}

void GIViewer::on_prev_clicked(GtkWidget *widget, gpointer user_data) {
    GIViewer *viewer = static_cast<GIViewer*>(user_data);
    viewer->current_image_index = (viewer->current_image_index - 1 + viewer->image_files.size()) % viewer->image_files.size();
    viewer->update_image();
}

void GIViewer::on_open_folder_clicked(GtkWidget *widget, gpointer user_data) {
    GIViewer *viewer = static_cast<GIViewer*>(user_data);

    GtkWidget *dialog = gtk_file_chooser_dialog_new("Select Folder", GTK_WINDOW(viewer->window),
                                                    GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, "_Cancel", GTK_RESPONSE_CANCEL,
                                                    "_Open", GTK_RESPONSE_ACCEPT, NULL);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *folder_name = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        viewer->load_images(folder_name);
        g_free(folder_name);
    }
    gtk_widget_destroy(dialog);
}

gboolean GIViewer::on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    GIViewer *viewer = static_cast<GIViewer*>(user_data);

    if (event->state & GDK_CONTROL_MASK) {
        if (event->keyval == GDK_KEY_plus || event->keyval == GDK_KEY_equal) {
            viewer->zoom_factor *= 1.1;  // Zoom in
            viewer->update_image();
            return TRUE;
        } else if (event->keyval == GDK_KEY_minus) {
            viewer->zoom_factor /= 1.1;  // Zoom out
            viewer->update_image();
            return TRUE;
        }
    }
    return FALSE;
}

gboolean GIViewer::on_scroll(GtkWidget *widget, GdkEventScroll *event, gpointer user_data) {
    GIViewer *viewer = static_cast<GIViewer*>(user_data);

    // Adjust zoom based on mouse wheel
    if (event->direction == GDK_SCROLL_UP) {
        viewer->zoom_factor *= 1.1;  // Zoom in
    } else if (event->direction == GDK_SCROLL_DOWN) {
        viewer->zoom_factor /= 1.1;  // Zoom out
    }

    // Update image
    viewer->update_image();
    return TRUE;
}

void GIViewer::run() {
    gtk_widget_add_events(window, GDK_SCROLL_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);
    g_signal_connect(window, "scroll-event", G_CALLBACK(on_scroll), this);
    gtk_main();
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    GIViewer viewer;
    viewer.run();
    return 0;
}
