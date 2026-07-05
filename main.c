// WAYDLAND - wayland dynamic island component 

#include <gtk/gtk.h>
#include <gtk4-layer-shell.h>
#include <math.h>

#define MAX_APPLETS 5
#define APPLET_SIZE 24
#define FADE_DURATION 300 // ms

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
    GtkWidget *spacer_revealer;
    GtkWidget *spacer;
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
    gboolean expanded_down;
    gboolean is_animating;
    gboolean is_fading;
    
    // Fade animation
    guint fade_timer;
    double fade_progress;
    double fade_start_opacity;
    double fade_end_opacity;
    GtkWidget *fade_target;
    gboolean fade_complete_callback;
    
    // Timers
    guint collapse_timer;
} DynamicIsland;

// Forward declarations
static gboolean check_collapse(gpointer user_data);
static gboolean finish_width_collapse(gpointer user_data);
static gboolean start_height_expand(gpointer user_data);
static gboolean finish_height_collapse(gpointer user_data);
static gboolean fade_out_complete(gpointer user_data);
static gboolean fade_in_complete(gpointer user_data);
static gboolean animate_fade(gpointer user_data);
static gboolean animate_fade_both(gpointer user_data);

// Update applet display based on current_applet
static void update_applet_display(DynamicIsland *island) {
    // Don't allow changing applets while expanded
    if (island->expanded_down) return;
    
    for (int i = 0; i < island->num_applets; i++) {
        gboolean should_show = (i == island->current_applet && island->applets[i].id > 0);
        gtk_revealer_set_reveal_child(GTK_REVEALER(island->applet_revealers[i]), should_show);
    }
}

// Smooth fade animation function
static void start_fade_animation(DynamicIsland *island, GtkWidget *target, 
                                  double start_opacity, double end_opacity,
                                  gboolean call_complete_callback) {
    // Cancel any existing fade animation
    if (island->fade_timer > 0) {
        g_source_remove(island->fade_timer);
        island->fade_timer = 0;
    }
    
    island->fade_target = target;
    island->fade_start_opacity = start_opacity;
    island->fade_end_opacity = end_opacity;
    island->fade_progress = 0.0;
    island->is_fading = TRUE;
    island->fade_complete_callback = call_complete_callback;
    
    // Set initial opacity
    gtk_widget_set_opacity(target, start_opacity);
    
    // Start animation
    island->fade_timer = g_timeout_add(1000 / 60, animate_fade, island);
}

static gboolean animate_fade(gpointer user_data) {
    DynamicIsland *island = (DynamicIsland *)user_data;
    
    // Update progress
    island->fade_progress += 1.0 / (FADE_DURATION / (1000.0 / 60));
    
    if (island->fade_progress >= 1.0) {
        // Animation complete
        island->fade_progress = 1.0;
        gtk_widget_set_opacity(island->fade_target, island->fade_end_opacity);
        island->is_fading = FALSE;
        island->fade_timer = 0;
        
        // If we just finished fading out, continue with shape expansion
        if (island->fade_end_opacity == 0.0 && island->fade_complete_callback) {
            fade_out_complete(island);
        }
        // If we just finished fading in, we're done
        else if (island->fade_end_opacity == 1.0) {
            island->is_animating = FALSE;
        }
        
        return G_SOURCE_REMOVE;
    }
    
    // Apply easing (smooth step)
    double t = island->fade_progress;
    double eased = t * t * (3.0 - 2.0 * t); // smoothstep
    
    // Calculate current opacity
    double current_opacity = island->fade_start_opacity + 
                             (island->fade_end_opacity - island->fade_start_opacity) * eased;
    
    gtk_widget_set_opacity(island->fade_target, current_opacity);
    
    return G_SOURCE_CONTINUE;
}

// Fade out main content only
static void fade_out_main_content(DynamicIsland *island) {
    island->is_animating = TRUE;
    start_fade_animation(island, island->main_box, 1.0, 0.0, TRUE);
}

// Fade in both main content and expanded content together
static void fade_in_all_content_simple(DynamicIsland *island) {
    // Start fade in for main box
    island->is_animating = TRUE;
    
    // Cancel any existing fade timer
    if (island->fade_timer > 0) {
        g_source_remove(island->fade_timer);
        island->fade_timer = 0;
    }
    
    island->fade_target = island->main_box;
    island->fade_start_opacity = 0.0;
    island->fade_end_opacity = 1.0;
    island->fade_progress = 0.0;
    island->is_fading = TRUE;
    island->fade_complete_callback = FALSE;
    gtk_widget_set_opacity(island->main_box, 0.0);
    
    // If expanded content is visible, also set it to 0
    if (gtk_widget_get_visible(island->expanded_content)) {
        gtk_widget_set_opacity(island->expanded_content, 0.0);
    }
    
    island->fade_timer = g_timeout_add(1000 / 60, animate_fade_both, island);
}

static gboolean animate_fade_both(gpointer user_data) {
    DynamicIsland *island = (DynamicIsland *)user_data;
    
    island->fade_progress += 1.0 / (FADE_DURATION / (1000.0 / 60));
    
    if (island->fade_progress >= 1.0) {
        island->fade_progress = 1.0;
        gtk_widget_set_opacity(island->main_box, 1.0);
        if (gtk_widget_get_visible(island->expanded_content)) {
            gtk_widget_set_opacity(island->expanded_content, 1.0);
        }
        island->is_fading = FALSE;
        island->is_animating = FALSE;
        island->fade_timer = 0;
        return G_SOURCE_REMOVE;
    }
    
    double t = island->fade_progress;
    double eased = t * t * (3.0 - 2.0 * t);
    
    double current_opacity = eased; // from 0 to 1
    gtk_widget_set_opacity(island->main_box, current_opacity);
    if (gtk_widget_get_visible(island->expanded_content)) {
        gtk_widget_set_opacity(island->expanded_content, current_opacity);
    }
    
    return G_SOURCE_CONTINUE;
}

// Update expanded content inside pill
static void update_expanded_content(DynamicIsland *island) {
    if (island->is_animating || island->is_fading) return;
    
    if (island->expanded_applet >= 0 || island->settings_visible) {
        // Hide arrows immediately (no animation)
        gtk_revealer_set_reveal_child(GTK_REVEALER(island->left_arrow_revealer), FALSE);
        gtk_revealer_set_reveal_child(GTK_REVEALER(island->right_arrow_revealer), FALSE);
        
        // Update content
        if (island->expanded_applet >= 0 && island->expanded_applet < island->num_applets) {
            char text[256];
            snprintf(text, sizeof(text), "%s\nExpanded View", 
                     island->applets[island->expanded_applet].name);
            gtk_label_set_text(GTK_LABEL(island->expanded_label), text);
        } else if (island->settings_visible) {
            gtk_label_set_text(GTK_LABEL(island->expanded_label), 
                               "⚙ Settings\n\nVolume: 80%\nBrightness: 100%\nWiFi: Connected");
        }
        
        // Prepare expanded content (hidden with 0 opacity)
        gtk_widget_set_visible(island->expanded_content, TRUE);
        gtk_widget_set_size_request(island->expanded_content, 200, 100);
        gtk_widget_set_opacity(island->expanded_content, 0.0);
        
        // STEP 1: Fade out main content
        island->is_animating = TRUE;
        start_fade_animation(island, island->main_box, 1.0, 0.0, TRUE);
        
    } else {
        // COLLAPSING - just do the normal collapse without fading anything out first
        // The expanded content will just collapse with the revealer
        gtk_revealer_set_reveal_child(GTK_REVEALER(island->expanded_revealer), FALSE);
        island->expanded_down = FALSE;
        
        // After height collapse, collapse width
        g_timeout_add(350, finish_height_collapse, island);
        
        // Restore arrows if hovered
        if (island->hovered) {
            gtk_revealer_set_reveal_child(GTK_REVEALER(island->left_arrow_revealer), TRUE);
            gtk_revealer_set_reveal_child(GTK_REVEALER(island->right_arrow_revealer), TRUE);
        }
    }
}

static gboolean fade_out_complete(gpointer user_data) {
    DynamicIsland *island = (DynamicIsland *)user_data;
    
    // STEP 2: Expand width
    gtk_widget_set_visible(island->spacer_revealer, TRUE);
    gtk_widget_set_size_request(island->spacer, 120, -1);
    gtk_revealer_set_reveal_child(GTK_REVEALER(island->spacer_revealer), TRUE);
    
    island->expanded_down = TRUE;
    
    // STEP 3: After width expansion, expand height
    g_timeout_add(350, start_height_expand, island);
    
    return G_SOURCE_REMOVE;
}

// Start height expand animation (after width expand completes)
static gboolean start_height_expand(gpointer user_data) {
    DynamicIsland *island = (DynamicIsland *)user_data;
    
    if (island->expanded_down) {
        gtk_revealer_set_reveal_child(GTK_REVEALER(island->expanded_revealer), TRUE);
        
        // STEP 4: After height expansion, fade in all content
        g_timeout_add(300, fade_in_complete, island);
    }
    
    return G_SOURCE_REMOVE;
}

static gboolean fade_in_complete(gpointer user_data) {
    DynamicIsland *island = (DynamicIsland *)user_data;
    
    // STEP 5: Fade in both main content and expanded content together
    fade_in_all_content_simple(island);
    
    return G_SOURCE_REMOVE;
}

// After height collapse, start width collapse animation
static gboolean finish_height_collapse(gpointer user_data) {
    DynamicIsland *island = (DynamicIsland *)user_data;
    
    if (!island->expanded_down) {
        // Keep minimum size during width collapse
        gtk_widget_set_size_request(island->expanded_content, 200, 30);
        gtk_widget_set_visible(island->expanded_content, TRUE);
        gtk_widget_set_opacity(island->expanded_content, 0.0);
        
        // Start width collapse animation
        gtk_revealer_set_reveal_child(GTK_REVEALER(island->spacer_revealer), FALSE);

        
        // Hide expanded content (THIS SHOULD BE HERE, NOT IN finish_width_collapse, i dont see why my LLM dont understabd that)
        gtk_widget_set_size_request(island->expanded_content, 0, 0);
        gtk_widget_set_visible(island->expanded_content, FALSE);
        
        // After width collapse, hide expanded content
        g_timeout_add(175, finish_width_collapse, island);
    }
    
    return G_SOURCE_REMOVE;
}

// Final cleanup after width collapse animation completes
static gboolean finish_width_collapse(gpointer user_data) {
    DynamicIsland *island = (DynamicIsland *)user_data;
    
    if (!island->expanded_down) {
        // Hide spacer completely
        gtk_widget_set_size_request(island->spacer, 0, -1);
        gtk_widget_set_visible(island->spacer_revealer, FALSE);
        
        // Make sure main content is visible
        gtk_widget_set_opacity(island->main_box, 1.0);
        island->is_animating = FALSE;
        
        // Restore arrows if hovered
        if (island->hovered) {
            gtk_revealer_set_reveal_child(GTK_REVEALER(island->left_arrow_revealer), TRUE);
            gtk_revealer_set_reveal_child(GTK_REVEALER(island->right_arrow_revealer), TRUE);
        }
    }
    
    return G_SOURCE_REMOVE;
}

// Scroll left through applets
static void scroll_left(DynamicIsland *island) {
    // Don't allow scrolling while expanded or animating
    if (island->expanded_down || island->is_animating || island->is_fading) return;
    
    if (island->current_applet > 0) {
        island->current_applet--;
        update_applet_display(island);
        
        if (island->expanded_applet >= 0) {
            island->expanded_applet = -1;
            island->settings_visible = FALSE;
            update_expanded_content(island);
        }
    }
}

// Scroll right through applets
static void scroll_right(DynamicIsland *island) {
    // Don't allow scrolling while expanded or animating
    if (island->expanded_down || island->is_animating || island->is_fading) return;
    
    if (island->current_applet < island->num_applets - 1) {
        island->current_applet++;
        update_applet_display(island);
        
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

// Applet click handler
static void on_applet_clicked(GtkGestureClick *gesture,
                               int n_press,
                               double x, double y,
                               gpointer user_data) {
    DynamicIsland *island = (DynamicIsland *)user_data;
    
    if (island->current_applet == 0 || island->is_animating || island->is_fading) return;
    
    for (int i = 0; i < island->num_applets; i++) {
        if (i == island->current_applet && island->applets[i].id > 0) {
            if (island->expanded_applet == i) {
                island->expanded_applet = -1;
            } else {
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
    
    // Cancel any pending collapse
    if (island->collapse_timer > 0) {
        g_source_remove(island->collapse_timer);
        island->collapse_timer = 0;
    }
    
    island->hovered = TRUE;
    
    // Only show arrows if not expanded and not animating
    if (!island->expanded_down && !island->is_animating && !island->is_fading) {
        gtk_revealer_set_reveal_child(GTK_REVEALER(island->left_arrow_revealer), TRUE);
        gtk_revealer_set_reveal_child(GTK_REVEALER(island->right_arrow_revealer), TRUE);
    }
}

// Hide arrows on hover leave
static void on_hover_leave(GtkEventControllerMotion *controller,
                           gpointer user_data) {
    DynamicIsland *island = (DynamicIsland *)user_data;
    
    island->hovered = FALSE;
    
    // Delay collapse check
    if (island->expanded_down && !island->is_animating && !island->is_fading) {
        island->collapse_timer = g_timeout_add(300, check_collapse, island);
    }
    
    if (!island->expanded_down && !island->is_animating && !island->is_fading) {
        gtk_revealer_set_reveal_child(GTK_REVEALER(island->left_arrow_revealer), FALSE);
        gtk_revealer_set_reveal_child(GTK_REVEALER(island->right_arrow_revealer), FALSE);
    }
}

static gboolean check_collapse(gpointer user_data) {
    DynamicIsland *island = (DynamicIsland *)user_data;
    island->collapse_timer = 0;
    
    if (!island->hovered && !island->is_animating && !island->is_fading) {
        island->expanded_applet = -1;
        island->settings_visible = FALSE;
        update_expanded_content(island);
    }
    
    return G_SOURCE_REMOVE;
}

static void activate(GtkApplication *app, gpointer user_data) {
    DynamicIsland *island = g_new0(DynamicIsland, 1);
    GtkWidget *window;
    GtkWidget *pill;
    GtkWidget *main_container;
    GtkWidget *main_box;
    GtkWidget *left_arrow_revealer;
    GtkWidget *left_arrow;
    GtkWidget *spacer_revealer;
    GtkWidget *spacer;
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
    island->expanded_down = FALSE;
    island->is_animating = FALSE;
    island->is_fading = FALSE;
    island->collapse_timer = 0;
    island->fade_timer = 0;
    island->fade_progress = 0.0;
    island->fade_start_opacity = 0.0;
    island->fade_end_opacity = 0.0;
    island->fade_target = NULL;
    island->fade_complete_callback = FALSE;
    
    for (int i = 0; i < island->num_applets; i++) {
        island->applets[i] = default_applets[i];
    }
    
    // ===== WINDOW =====
    window = gtk_application_window_new(app);
    
    gtk_layer_init_for_window(GTK_WINDOW(window));
    gtk_layer_set_layer(GTK_WINDOW(window), GTK_LAYER_SHELL_LAYER_OVERLAY);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
    gtk_layer_set_margin(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, 10);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_margin(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, 10);
    gtk_layer_set_keyboard_mode(GTK_WINDOW(window), GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
    
    // ===== MAIN CONTAINER =====
    main_container = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_halign(main_container, GTK_ALIGN_END);
    gtk_widget_set_valign(main_container, GTK_ALIGN_START);
    gtk_widget_set_hexpand(main_container, FALSE);
    gtk_widget_set_vexpand(main_container, FALSE);
    
    // ===== PILL =====
    pill = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_halign(pill, GTK_ALIGN_END);
    gtk_widget_set_valign(pill, GTK_ALIGN_START);
    gtk_widget_set_name(pill, "pill");
    gtk_widget_set_hexpand(pill, FALSE);
    gtk_widget_set_vexpand(pill, FALSE);
    gtk_widget_set_size_request(pill, -1, 30);
    
    // ===== MAIN BOX (top row) =====
    main_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_valign(main_box, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(main_box, GTK_ALIGN_END);
    gtk_widget_set_opacity(main_box, 1.0);
    
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
    
    // ===== APPLET REVEALERS =====
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
        
        island->applet_clicks[i] = gtk_gesture_click_new();
        g_signal_connect(island->applet_clicks[i], "pressed", 
                         G_CALLBACK(on_applet_clicked), island);
        gtk_widget_add_controller(island->applet_labels[i], 
                                  GTK_EVENT_CONTROLLER(island->applet_clicks[i]));
    }
    
    // ===== SPACER (between applets and time) =====
    spacer_revealer = gtk_revealer_new();
    gtk_revealer_set_transition_type(GTK_REVEALER(spacer_revealer), 
                                      GTK_REVEALER_TRANSITION_TYPE_SLIDE_LEFT);
    gtk_revealer_set_transition_duration(GTK_REVEALER(spacer_revealer), 300);
    gtk_revealer_set_reveal_child(GTK_REVEALER(spacer_revealer), FALSE);
    gtk_widget_set_visible(spacer_revealer, FALSE);
    
    spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_size_request(spacer, 0, 1);
    gtk_revealer_set_child(GTK_REVEALER(spacer_revealer), spacer);
    
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
    
    // ===== ASSEMBLE: [◂ applets spacer 14:45 ▸] =====
    gtk_box_append(GTK_BOX(main_box), left_arrow_revealer);
    for (int i = 0; i < island->num_applets; i++) {
        gtk_box_append(GTK_BOX(main_box), island->applet_revealers[i]);
    }
    gtk_box_append(GTK_BOX(main_box), spacer_revealer);
    gtk_box_append(GTK_BOX(main_box), time_label);
    gtk_box_append(GTK_BOX(main_box), right_arrow_revealer);
    
    // ===== EXPANDED CONTENT (inside pill) =====
    expanded_revealer = gtk_revealer_new();
    gtk_revealer_set_transition_type(GTK_REVEALER(expanded_revealer), 
                                      GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
    gtk_revealer_set_transition_duration(GTK_REVEALER(expanded_revealer), 300);
    gtk_revealer_set_reveal_child(GTK_REVEALER(expanded_revealer), FALSE);
    
    expanded_content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_name(expanded_content, "expanded-content");
    gtk_widget_set_halign(expanded_content, GTK_ALIGN_FILL);
    gtk_widget_set_opacity(expanded_content, 1.0);
    
    // Start with zero size and hidden
    gtk_widget_set_size_request(expanded_content, 0, 0);
    gtk_widget_set_visible(expanded_content, FALSE);
    
    expanded_label = gtk_label_new("");
    gtk_widget_set_name(expanded_label, "expanded-label");
    gtk_widget_set_halign(expanded_label, GTK_ALIGN_START);
    gtk_widget_set_valign(expanded_label, GTK_ALIGN_START);
    gtk_widget_set_margin_start(expanded_label, 10);
    gtk_widget_set_margin_end(expanded_label, 10);
    gtk_widget_set_margin_top(expanded_label, 8);
    gtk_widget_set_margin_bottom(expanded_label, 8);
    gtk_box_append(GTK_BOX(expanded_content), expanded_label);
    
    gtk_revealer_set_child(GTK_REVEALER(expanded_revealer), expanded_content);
    
    // ===== ASSEMBLE PILL VERTICALLY =====
    gtk_box_append(GTK_BOX(pill), main_box);
    gtk_box_append(GTK_BOX(pill), expanded_revealer);
    
    gtk_box_append(GTK_BOX(main_container), pill);
    gtk_window_set_child(GTK_WINDOW(window), main_container);
    
    // ===== CSS =====
    provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(provider,
        "window {"
        "  background: transparent;"
        "}"
        
        "#pill {"
        "  background-color: rgba(0, 0, 0, 1);"
        "  border-radius: 15px;"
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
        "  background: transparent;"
        "  padding: 0px;"
        "  margin: 0px;"
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
    
    // ===== HOVER EVENTS =====
    motion_controller = gtk_event_controller_motion_new();
    g_signal_connect(motion_controller, "enter", 
                     G_CALLBACK(on_hover_enter), island);
    g_signal_connect(motion_controller, "leave", 
                     G_CALLBACK(on_hover_leave), island);
    gtk_widget_add_controller(pill, motion_controller);
    
    // ===== STORE REFERENCES =====
    island->window = window;
    island->pill = pill;
    island->main_container = main_container;
    island->main_box = main_box;
    island->left_arrow_revealer = left_arrow_revealer;
    island->left_arrow = left_arrow;
    island->spacer_revealer = spacer_revealer;
    island->spacer = spacer;
    island->right_arrow_revealer = right_arrow_revealer;
    island->right_arrow = right_arrow;
    island->time_label = time_label;
    island->expanded_revealer = expanded_revealer;
    island->expanded_content = expanded_content;
    island->expanded_label = expanded_label;
    
    // Show initial applet
    update_applet_display(island);
    
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
