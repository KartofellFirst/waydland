
// WAYDLAND - wayland dynamic island component 

#include <gtk/gtk.h>
#include <gtk4-layer-shell.h>

#define MAX_APPLETS 5
#define APPLET_SIZE 24

typedef struct {
    int id;
    const char *icon;
    const char *name;
} Applet;

typedef struct {
    GtkWidget *window;
    GtkWidget *pill;
    GtkWidget *main_container;
    GtkWidget *main_box;
    
    GtkWidget *left_arrow_revealer;
    GtkWidget *left_arrow;
    GtkWidget *time_label;
    GtkWidget *right_arrow_revealer;
    GtkWidget *right_arrow;
    
    GtkWidget *expanded_revealer;
    GtkWidget *expanded_content;
    GtkWidget *expanded_label;
    
    // Applet widgets
    GtkWidget *applet_revealers[MAX_APPLETS];
    GtkWidget *applet_labels[MAX_APPLETS];
    GtkGesture *applet_clicks[MAX_APPLETS];
    
    Applet applets[MAX_APPLETS];
    int num_applets;
    int current_applet;
    int expanded_applet;
    gboolean hovered;
    gboolean settings_visible;
} DynamicIsland;

// Update applet display based on current_applet
static void update_applet_display(DynamicIsland *island) {
    for (int i = 0; i < island->num_applets; i++) {
        // Only show applet if it's the current one AND it's not empty (id > 0)
        gboolean should_show = (i == island->current_applet && island->applets[i].id > 0);
        gtk_revealer_set_reveal_child(GTK_REVEALER(island->applet_revealers[i]), should_show);
    }
}

// Update expanded content
static void update_expanded_content(DynamicIsland *island) {
    if (island->expanded_applet >= 0 && island->expanded_applet < island->num_applets) {
        char text[256];
        snprintf(text, sizeof(text), "%s - Expanded View", 
                 island->applets[island->expanded_applet].name);
        gtk_label_set_text(GTK_LABEL(island->expanded_label), text);
        gtk_revealer_set_reveal_child(GTK_REVEALER(island->expanded_revealer), TRUE);
    } else if (island->settings_visible) {
        gtk_label_set_text(GTK_LABEL(island->expanded_label), "Settings Panel");
        gtk_revealer_set_reveal_child(GTK_REVEALER(island->expanded_revealer), TRUE);
    } else {
        gtk_revealer_set_reveal_child(GTK_REVEALER(island->expanded_revealer), FALSE);
    }
}

// Scroll left through applets
static void scroll_left(DynamicIsland *island) {
    if (island->current_applet > 0) {
        island->current_applet--;
        update_applet_display(island);
        
        // Collapse any expanded content
        if (island->expanded_applet >= 0) {
            island->expanded_applet = -1;
            island->settings_visible = FALSE;
            update_expanded_content(island);
        }
    }
}

// Scroll right through applets
static void scroll_right(DynamicIsland *island) {
    if (island->current_applet < island->num_applets - 1) {
        island->current_applet++;
        update_applet_display(island);
        
        // Collapse any expanded content
        if (island->expanded_applet >= 0) {
            island->expanded_applet = -1;
            island->settings_visible = FALSE;
            update_expanded_content(island);
        }
    } else if (island->current_applet == island->num_applets - 1) {
        // At last applet, toggle settings
        island->settings_visible = !island->settings_visible;
        island->expanded_applet = -1;
        update_expanded_content(island);
    }
}

// Arrow click handlers
static void on_left_arrow_clicked(GtkGestureClick *gesture,
                                   int n_press,
                                   double x, double y,
                                   gpointer user_data) {
    DynamicIsland *island = (DynamicIsland *)user_data;
    scroll_left(island);
}

static void on_right_arrow_clicked(GtkGestureClick *gesture,
                                    int n_press,
                                    double x, double y,
                                    gpointer user_data) {
    DynamicIsland *island = (DynamicIsland *)user_data;
    scroll_right(island);
}

// Applet click handler - only works for non-empty applets
static void on_applet_clicked(GtkGestureClick *gesture,
                               int n_press,
                               double x, double y,
                               gpointer user_data) {
    DynamicIsland *island = (DynamicIsland *)user_data;
    
    // Don't respond to clicks on empty applet (id 0)
    if (island->current_applet == 0) return;
    
    for (int i = 0; i < island->num_applets; i++) {
        if (i == island->current_applet && island->applets[i].id > 0) {
            if (island->expanded_applet == i) {
                // Toggle off
                island->expanded_applet = -1;
            } else {
                // Expand this applet
                island->expanded_applet = i;
                island->settings_visible = FALSE;
            }
            update_expanded_content(island);
            break;
        }
    }
}

// Show arrows on hover
static void on_hover_enter(GtkEventControllerMotion *controller,
                           double x, double y,
                           gpointer user_data) {
    DynamicIsland *island = (DynamicIsland *)user_data;
    
    island->hovered = TRUE;
    gtk_revealer_set_reveal_child(GTK_REVEALER(island->left_arrow_revealer), TRUE);
    gtk_revealer_set_reveal_child(GTK_REVEALER(island->right_arrow_revealer), TRUE);
}

// Hide arrows on hover leave
static void on_hover_leave(GtkEventControllerMotion *controller,
                           gpointer user_data) {
    DynamicIsland *island = (DynamicIsland *)user_data;
    
    island->hovered = FALSE;
    gtk_revealer_set_reveal_child(GTK_REVEALER(island->left_arrow_revealer), FALSE);
    gtk_revealer_set_reveal_child(GTK_REVEALER(island->right_arrow_revealer), FALSE);
    
    // Collapse expanded content on leave
    if (island->expanded_applet >= 0 || island->settings_visible) {
        island->expanded_applet = -1;
        island->settings_visible = FALSE;
        update_expanded_content(island);
    }
    
    // IMPORTANT: Keep current applet selection when unhovering
    // Only reset to 0 if we were on empty applet anyway
    // This preserves the applet selection between hover sessions
}

static void activate(GtkApplication *app, gpointer user_data) {
    DynamicIsland *island = g_new0(DynamicIsland, 1);
    GtkWidget *window;
    GtkWidget *pill;
    GtkWidget *main_container;
    GtkWidget *main_box;
    GtkWidget *left_arrow_revealer;
    GtkWidget *left_arrow;
    GtkWidget *time_label;
    GtkWidget *right_arrow_revealer;
    GtkWidget *right_arrow;
    GtkWidget *expanded_revealer;
    GtkWidget *expanded_content;
    GtkWidget *expanded_label;
    GtkCssProvider *provider;
    GtkEventController *motion_controller;
    GtkGesture *left_click;
    GtkGesture *right_click;
    
    // Initialize applets
    Applet default_applets[] = {
        {0, "", "Empty"},
        {1, "♥", "Heart"},
        {2, "★", "Star"},
        {3, "♪", "Music"},
        {4, "⚙", "Settings"}
    };
    
    island->num_applets = 5;
    island->current_applet = 0;
    island->expanded_applet = -1;
    island->hovered = FALSE;
    island->settings_visible = FALSE;
    
    for (int i = 0; i < island->num_applets; i++) {
        island->applets[i] = default_applets[i];
    }
    
    // ===== WINDOW =====
    window = gtk_application_window_new(app);
    gtk_widget_set_size_request(window, 80, 30);
    
    gtk_layer_init_for_window(GTK_WINDOW(window));
    gtk_layer_set_layer(GTK_WINDOW(window), GTK_LAYER_SHELL_LAYER_OVERLAY);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
    gtk_layer_set_margin(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, 10);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_margin(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, 10);
    gtk_layer_set_keyboard_mode(GTK_WINDOW(window), GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
    
    // ===== MAIN CONTAINER =====
    main_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_halign(main_container, GTK_ALIGN_END);
    
    // ===== PILL =====
    pill = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_halign(pill, GTK_ALIGN_END);
    gtk_widget_set_valign(pill, GTK_ALIGN_CENTER);
    gtk_widget_set_name(pill, "pill");
    
    // ===== MAIN BOX =====
    main_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_valign(main_box, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(main_box, GTK_ALIGN_CENTER);
    
    // ===== LEFT ARROW =====
    left_arrow_revealer = gtk_revealer_new();
    gtk_revealer_set_transition_type(GTK_REVEALER(left_arrow_revealer), 
                                      GTK_REVEALER_TRANSITION_TYPE_SLIDE_RIGHT);
    gtk_revealer_set_transition_duration(GTK_REVEALER(left_arrow_revealer), 250);
    gtk_revealer_set_reveal_child(GTK_REVEALER(left_arrow_revealer), FALSE);
    
    left_arrow = gtk_label_new("◂");
    gtk_widget_set_name(left_arrow, "arrow");
    gtk_widget_set_valign(left_arrow, GTK_ALIGN_CENTER);
    gtk_revealer_set_child(GTK_REVEALER(left_arrow_revealer), left_arrow);
    
    left_click = gtk_gesture_click_new();
    g_signal_connect(left_click, "pressed", G_CALLBACK(on_left_arrow_clicked), island);
    gtk_widget_add_controller(left_arrow, GTK_EVENT_CONTROLLER(left_click));
    
    // ===== APPLET REVEALERS (LEFT of time) =====
    for (int i = 0; i < island->num_applets; i++) {
        island->applet_revealers[i] = gtk_revealer_new();
        gtk_revealer_set_transition_type(GTK_REVEALER(island->applet_revealers[i]), 
                                          GTK_REVEALER_TRANSITION_TYPE_SLIDE_RIGHT);
        gtk_revealer_set_transition_duration(GTK_REVEALER(island->applet_revealers[i]), 200);
        gtk_revealer_set_reveal_child(GTK_REVEALER(island->applet_revealers[i]), FALSE);
        
        island->applet_labels[i] = gtk_label_new(island->applets[i].icon);
        gtk_widget_set_name(island->applet_labels[i], "applet");
        gtk_widget_set_valign(island->applet_labels[i], GTK_ALIGN_CENTER);
        gtk_widget_set_size_request(island->applet_labels[i], APPLET_SIZE, APPLET_SIZE);
        
        gtk_revealer_set_child(GTK_REVEALER(island->applet_revealers[i]), 
                               island->applet_labels[i]);
        
        // Make applet clickable
        island->applet_clicks[i] = gtk_gesture_click_new();
        g_signal_connect(island->applet_clicks[i], "pressed", 
                         G_CALLBACK(on_applet_clicked), island);
        gtk_widget_add_controller(island->applet_labels[i], 
                                  GTK_EVENT_CONTROLLER(island->applet_clicks[i]));
    }
    
    // ===== TIME LABEL =====
    time_label = gtk_label_new("14:45");
    gtk_widget_set_name(time_label, "time");
    gtk_widget_set_valign(time_label, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_start(time_label, 4);
    gtk_widget_set_margin_end(time_label, 4);
    
    // ===== RIGHT ARROW =====
    right_arrow_revealer = gtk_revealer_new();
    gtk_revealer_set_transition_type(GTK_REVEALER(right_arrow_revealer), 
                                      GTK_REVEALER_TRANSITION_TYPE_SLIDE_LEFT);
    gtk_revealer_set_transition_duration(GTK_REVEALER(right_arrow_revealer), 250);
    gtk_revealer_set_reveal_child(GTK_REVEALER(right_arrow_revealer), FALSE);
    
    right_arrow = gtk_label_new("▸");
    gtk_widget_set_name(right_arrow, "arrow");
    gtk_widget_set_valign(right_arrow, GTK_ALIGN_CENTER);
    gtk_revealer_set_child(GTK_REVEALER(right_arrow_revealer), right_arrow);
    
    right_click = gtk_gesture_click_new();
    g_signal_connect(right_click, "pressed", G_CALLBACK(on_right_arrow_clicked), island);
    gtk_widget_add_controller(right_arrow, GTK_EVENT_CONTROLLER(right_click));
    
    // ===== ASSEMBLE: [◂ applets 14:45 ▸] =====
    gtk_box_append(GTK_BOX(main_box), left_arrow_revealer);
    for (int i = 0; i < island->num_applets; i++) {
        gtk_box_append(GTK_BOX(main_box), island->applet_revealers[i]);
    }
    gtk_box_append(GTK_BOX(main_box), time_label);
    gtk_box_append(GTK_BOX(main_box), right_arrow_revealer);
    
    gtk_box_append(GTK_BOX(pill), main_box);
    
    // ===== EXPANDED CONTENT =====
    expanded_revealer = gtk_revealer_new();
    gtk_revealer_set_transition_type(GTK_REVEALER(expanded_revealer), 
                                      GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
    gtk_revealer_set_transition_duration(GTK_REVEALER(expanded_revealer), 300);
    gtk_revealer_set_reveal_child(GTK_REVEALER(expanded_revealer), FALSE);
    
    expanded_content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_name(expanded_content, "expanded-content");
    gtk_widget_set_size_request(expanded_content, 200, 100);
    
    expanded_label = gtk_label_new("");
    gtk_widget_set_name(expanded_label, "expanded-label");
    gtk_box_append(GTK_BOX(expanded_content), expanded_label);
    
    gtk_revealer_set_child(GTK_REVEALER(expanded_revealer), expanded_content);
    
    // ===== ASSEMBLE VERTICAL =====
    gtk_box_append(GTK_BOX(main_container), pill);
    gtk_box_append(GTK_BOX(main_container), expanded_revealer);
    
    gtk_window_set_child(GTK_WINDOW(window), main_container);
    
    // ===== CSS =====
    provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(provider,
        "window {"
        "  background: transparent;"
        "}"
        
        "#pill {"
        "  background-color: rgba(0, 0, 0, 1);"
        "  border-radius: 20px;"
        "  padding: 3px;"
        "  min-height: 24px;"
        "}"
        
        "#time {"
        "  color: white;"
        "  font-size: 13px;"
        "  font-weight: bold;"
        "  font-family: monospace;"
        "  min-width: 42px;"
        "}"
        
        "#arrow {"
        "  color: rgba(255, 255, 255, 1);"
        "  font-size: 11px;"
        "  min-width: 14px;"
        "}"
        
        "#arrow:hover {"
        "  color: rgba(255, 255, 255, 0.7);"
        "}"
        
        "#applet {"
        "  color: white;"
        "  font-size: 14px;"
        "  background: rgba(255, 255, 255, 0);"
        "  border-radius: 12px;"
        "}"
        
        "#applet:hover {"
        "  background: rgba(255, 255, 255, 0.2);"
        "}"
        
        "#expanded-content {"
        "  background-color: rgba(0, 0, 0, 0.9);"
        "  border-radius: 12px;"
        "  padding: 10px;"
        "  margin-top: 4px;"
        "}"
        
        "#expanded-label {"
        "  color: white;"
        "  font-size: 12px;"
        "}"
    );
    
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    
    // ===== HOVER EVENTS ON PILL =====
    motion_controller = gtk_event_controller_motion_new();
    g_signal_connect(motion_controller, "enter", 
                     G_CALLBACK(on_hover_enter), island);
    g_signal_connect(motion_controller, "leave", 
                     G_CALLBACK(on_hover_leave), island);
    gtk_widget_add_controller(pill, motion_controller);
    
    // ===== INIT STATE =====
    island->window = window;
    island->pill = pill;
    island->main_container = main_container;
    island->main_box = main_box;
    island->left_arrow_revealer = left_arrow_revealer;
    island->left_arrow = left_arrow;
    island->right_arrow_revealer = right_arrow_revealer;
    island->right_arrow = right_arrow;
    island->time_label = time_label;
    island->expanded_revealer = expanded_revealer;
    island->expanded_content = expanded_content;
    island->expanded_label = expanded_label;
    
    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
    GtkApplication *app;
    int status;
    
    app = gtk_application_new("com.github.waydland", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    
    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    
    return status;
}
