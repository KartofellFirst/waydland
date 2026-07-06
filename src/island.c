#include "island.h"
#include <string.h>

// ===== FORWARD DECLARATIONS (чтобы функции видели друг друга) =====
static gboolean animate_fade(gpointer user_data);
static gboolean animate_fade_both(gpointer user_data);
static void start_fade_animation(DynamicIsland *island, GtkWidget *target, 
                                  double start_opacity, double end_opacity,
                                  gboolean call_complete_callback);
static void fade_out_main_content(DynamicIsland *island);
static void fade_in_all_content_simple(DynamicIsland *island);
static gboolean fade_out_complete(gpointer user_data);
static gboolean start_height_expand(gpointer user_data);
static gboolean fade_in_complete(gpointer user_data);
static gboolean finish_height_collapse(gpointer user_data);
static gboolean finish_width_collapse(gpointer user_data);
static gboolean check_collapse(gpointer user_data);
static void update_applet_display(DynamicIsland *island);
static void update_expanded_content(DynamicIsland *island);

// ===== АНИМАЦИЯ (СНАЧАЛА ИДЕТ ОПРЕДЕЛЕНИЕ) =====

static gboolean animate_fade(gpointer user_data) {
    DynamicIsland *island = (DynamicIsland *)user_data;
    
    island->fade_progress += 1.0 / (FADE_DURATION / (1000.0 / 60));
    
    if (island->fade_progress >= 1.0) {
        island->fade_progress = 1.0;
        gtk_widget_set_opacity(island->fade_target, island->fade_end_opacity);
        island->is_fading = FALSE;
        island->fade_timer = 0;
        
        if (island->fade_end_opacity == 0.0 && island->fade_complete_callback) {
            g_timeout_add(10, (GSourceFunc)fade_out_complete, island);
        } else if (island->fade_end_opacity == 1.0) {
            island->is_animating = FALSE;
        }
        
        return G_SOURCE_REMOVE;
    }
    
    double t = island->fade_progress;
    double eased = t * t * (3.0 - 2.0 * t);
    
    double current_opacity = island->fade_start_opacity + 
                             (island->fade_end_opacity - island->fade_start_opacity) * eased;
    
    gtk_widget_set_opacity(island->fade_target, current_opacity);
    
    return G_SOURCE_CONTINUE;
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
    
    double current_opacity = eased;
    gtk_widget_set_opacity(island->main_box, current_opacity);
    if (gtk_widget_get_visible(island->expanded_content)) {
        gtk_widget_set_opacity(island->expanded_content, current_opacity);
    }
    
    return G_SOURCE_CONTINUE;
}

static void start_fade_animation(DynamicIsland *island, GtkWidget *target, 
                                  double start_opacity, double end_opacity,
                                  gboolean call_complete_callback) {
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
    
    gtk_widget_set_opacity(target, start_opacity);
    
    island->fade_timer = g_timeout_add(1000 / 60, animate_fade, island);
}

static void fade_out_main_content(DynamicIsland *island) {
    island->is_animating = TRUE;
    start_fade_animation(island, island->main_box, 1.0, 0.0, TRUE);
}

static void fade_in_all_content_simple(DynamicIsland *island) {
    island->is_animating = TRUE;
    
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
    
    if (gtk_widget_get_visible(island->expanded_content)) {
        gtk_widget_set_opacity(island->expanded_content, 0.0);
    }
    
    island->fade_timer = g_timeout_add(1000 / 60, animate_fade_both, island);
}

static gboolean fade_out_complete(gpointer user_data) {
    DynamicIsland *island = (DynamicIsland *)user_data;
    
    gtk_widget_set_visible(island->spacer_revealer, TRUE);
    gtk_widget_set_size_request(island->spacer, 120, -1);
    gtk_revealer_set_reveal_child(GTK_REVEALER(island->spacer_revealer), TRUE);
    
    island->expanded_down = TRUE;
    
    g_timeout_add(350, (GSourceFunc)start_height_expand, island);
    
    return G_SOURCE_REMOVE;
}

static gboolean start_height_expand(gpointer user_data) {
    DynamicIsland *island = (DynamicIsland *)user_data;
    
    if (island->expanded_down) {
        gtk_revealer_set_reveal_child(GTK_REVEALER(island->expanded_revealer), TRUE);
        g_timeout_add(300, (GSourceFunc)fade_in_complete, island);
    }
    
    return G_SOURCE_REMOVE;
}

static gboolean fade_in_complete(gpointer user_data) {
    DynamicIsland *island = (DynamicIsland *)user_data;
    fade_in_all_content_simple(island);
    return G_SOURCE_REMOVE;
}

static gboolean finish_height_collapse(gpointer user_data) {
    DynamicIsland *island = (DynamicIsland *)user_data;
    
    if (!island->expanded_down) {
        gtk_widget_set_size_request(island->expanded_content, 200, 30);
        gtk_widget_set_visible(island->expanded_content, TRUE);
        gtk_widget_set_opacity(island->expanded_content, 0.0);
        
        gtk_revealer_set_reveal_child(GTK_REVEALER(island->spacer_revealer), FALSE);
        
        gtk_widget_set_size_request(island->expanded_content, 0, 0);
        gtk_widget_set_visible(island->expanded_content, FALSE);
        
        g_timeout_add(175, (GSourceFunc)finish_width_collapse, island);
    }
    
    return G_SOURCE_REMOVE;
}

static gboolean finish_width_collapse(gpointer user_data) {
    DynamicIsland *island = (DynamicIsland *)user_data;
    
    if (!island->expanded_down) {
        gtk_widget_set_size_request(island->spacer, 0, -1);
        gtk_widget_set_visible(island->spacer_revealer, FALSE);
        
        gtk_widget_set_opacity(island->main_box, 1.0);
        island->is_animating = FALSE;
        
        if (island->hovered) {
            gtk_revealer_set_reveal_child(GTK_REVEALER(island->left_arrow_revealer), TRUE);
            gtk_revealer_set_reveal_child(GTK_REVEALER(island->right_arrow_revealer), TRUE);
        }
    }
    
    return G_SOURCE_REMOVE;
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

// ===== ОБНОВЛЕНИЕ ОТОБРАЖЕНИЯ =====

static void update_applet_display(DynamicIsland *island) {
    if (island->expanded_down) return;
    
    for (int i = 0; i < island->num_applets; i++) {
        gboolean should_show = (i == island->current_applet && island->applets[i].id > 0);
        gtk_revealer_set_reveal_child(GTK_REVEALER(island->applet_revealers[i]), should_show);
    }
}

static void update_expanded_content(DynamicIsland *island) {
    if (island->is_animating || island->is_fading) return;
    
    if (island->expanded_applet >= 0 || island->settings_visible) {
        gtk_revealer_set_reveal_child(GTK_REVEALER(island->left_arrow_revealer), FALSE);
        gtk_revealer_set_reveal_child(GTK_REVEALER(island->right_arrow_revealer), FALSE);
        
        if (island->expanded_applet >= 0 && island->expanded_applet < island->num_applets) {
            char text[256];
            snprintf(text, sizeof(text), "%s\nExpanded View", 
                     island->applets[island->expanded_applet].name);
            gtk_label_set_text(GTK_LABEL(island->expanded_label), text);
        } else if (island->settings_visible) {
            gtk_label_set_text(GTK_LABEL(island->expanded_label), 
                               "⚙ Settings\n\nVolume: 80%\nBrightness: 100%\nWiFi: Connected");
        }
        
        gtk_widget_set_visible(island->expanded_content, TRUE);
        gtk_widget_set_size_request(island->expanded_content, 200, 100);
        gtk_widget_set_opacity(island->expanded_content, 0.0);
        
        island->is_animating = TRUE;
        start_fade_animation(island, island->main_box, 1.0, 0.0, TRUE);
        
    } else {
        gtk_revealer_set_reveal_child(GTK_REVEALER(island->expanded_revealer), FALSE);
        island->expanded_down = FALSE;
        
        g_timeout_add(350, (GSourceFunc)finish_height_collapse, island);
        
        if (island->hovered) {
            gtk_revealer_set_reveal_child(GTK_REVEALER(island->left_arrow_revealer), TRUE);
            gtk_revealer_set_reveal_child(GTK_REVEALER(island->right_arrow_revealer), TRUE);
        }
    }
}

// ===== ПУБЛИЧНЫЕ ФУНКЦИИ ДЛЯ НАВИГАЦИИ =====

void scroll_left(DynamicIsland *island) {
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

void scroll_right(DynamicIsland *island) {
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
        island->settings_visible = !island->settings_visible;
        island->expanded_applet = -1;
        update_expanded_content(island);
    }
}

// ===== ОБРАБОТЧИКИ СОБЫТИЙ =====

void on_left_arrow_clicked(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data) {
    (void)gesture; (void)n_press; (void)x; (void)y;
    DynamicIsland *island = (DynamicIsland *)user_data;
    scroll_left(island);
}

void on_right_arrow_clicked(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data) {
    (void)gesture; (void)n_press; (void)x; (void)y;
    DynamicIsland *island = (DynamicIsland *)user_data;
    scroll_right(island);
}

void on_applet_clicked(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data) {
    (void)gesture; (void)n_press; (void)x; (void)y;
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

void on_hover_enter(GtkEventControllerMotion *controller, double x, double y, gpointer user_data) {
    (void)controller; (void)x; (void)y;
    DynamicIsland *island = (DynamicIsland *)user_data;
    
    if (island->collapse_timer > 0) {
        g_source_remove(island->collapse_timer);
        island->collapse_timer = 0;
    }
    
    island->hovered = TRUE;
    
    if (!island->expanded_down && !island->is_animating && !island->is_fading) {
        gtk_revealer_set_reveal_child(GTK_REVEALER(island->left_arrow_revealer), TRUE);
        gtk_revealer_set_reveal_child(GTK_REVEALER(island->right_arrow_revealer), TRUE);
    }
}

void on_hover_leave(GtkEventControllerMotion *controller, gpointer user_data) {
    (void)controller;
    DynamicIsland *island = (DynamicIsland *)user_data;
    
    island->hovered = FALSE;
    
    if (island->expanded_down && !island->is_animating && !island->is_fading) {
        island->collapse_timer = g_timeout_add(300, (GSourceFunc)check_collapse, island);
    }
    
    if (!island->expanded_down && !island->is_animating && !island->is_fading) {
        gtk_revealer_set_reveal_child(GTK_REVEALER(island->left_arrow_revealer), FALSE);
        gtk_revealer_set_reveal_child(GTK_REVEALER(island->right_arrow_revealer), FALSE);
    }
}

// ===== СОЗДАНИЕ ОСТРОВА =====

DynamicIsland* island_create(GtkApplication *app) {
    DynamicIsland *island = g_new0(DynamicIsland, 1);
    
    // Инициализация структуры
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
    
    for (int i = 0; i < island->num_applets; i++) {
        island->applets[i] = default_applets[i];
    }
    
    // === СОЗДАНИЕ ОКНА ===
    GtkWidget *window = gtk_application_window_new(app);
    gtk_layer_init_for_window(GTK_WINDOW(window));
    gtk_layer_set_layer(GTK_WINDOW(window), GTK_LAYER_SHELL_LAYER_OVERLAY);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
    gtk_layer_set_margin(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, 10);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_margin(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, 10);
    gtk_layer_set_keyboard_mode(GTK_WINDOW(window), GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
    
    island->window = window;
    
    // === MAIN CONTAINER ===
    GtkWidget *main_container = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_halign(main_container, GTK_ALIGN_END);
    gtk_widget_set_valign(main_container, GTK_ALIGN_START);
    gtk_widget_set_hexpand(main_container, FALSE);
    gtk_widget_set_vexpand(main_container, FALSE);
    island->main_container = main_container;
    
    // === PILL ===
    GtkWidget *pill = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_halign(pill, GTK_ALIGN_END);
    gtk_widget_set_valign(pill, GTK_ALIGN_START);
    gtk_widget_set_name(pill, "pill");
    gtk_widget_set_hexpand(pill, FALSE);
    gtk_widget_set_vexpand(pill, FALSE);
    gtk_widget_set_size_request(pill, -1, 30);
    island->pill = pill;
    
    // === MAIN BOX ===
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_valign(main_box, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(main_box, GTK_ALIGN_END);
    gtk_widget_set_opacity(main_box, 1.0);
    island->main_box = main_box;
    
    // === LEFT ARROW ===
    GtkWidget *left_arrow_revealer = gtk_revealer_new();
    gtk_revealer_set_transition_type(GTK_REVEALER(left_arrow_revealer), 
                                      GTK_REVEALER_TRANSITION_TYPE_SLIDE_RIGHT);
    gtk_revealer_set_transition_duration(GTK_REVEALER(left_arrow_revealer), 250);
    gtk_revealer_set_reveal_child(GTK_REVEALER(left_arrow_revealer), FALSE);
    island->left_arrow_revealer = left_arrow_revealer;
    
    GtkWidget *left_arrow = gtk_label_new("◂");
    gtk_widget_set_name(left_arrow, "arrow");
    gtk_widget_set_valign(left_arrow, GTK_ALIGN_CENTER);
    gtk_revealer_set_child(GTK_REVEALER(left_arrow_revealer), left_arrow);
    island->left_arrow = left_arrow;
    
    GtkGesture *left_click = gtk_gesture_click_new();
    g_signal_connect(left_click, "pressed", G_CALLBACK(on_left_arrow_clicked), island);
    gtk_widget_add_controller(left_arrow, GTK_EVENT_CONTROLLER(left_click));
    
    // === APPLET REVEALERS ===
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
    
    // === SPACER ===
    GtkWidget *spacer_revealer = gtk_revealer_new();
    gtk_revealer_set_transition_type(GTK_REVEALER(spacer_revealer), 
                                      GTK_REVEALER_TRANSITION_TYPE_SLIDE_LEFT);
    gtk_revealer_set_transition_duration(GTK_REVEALER(spacer_revealer), 300);
    gtk_revealer_set_reveal_child(GTK_REVEALER(spacer_revealer), FALSE);
    gtk_widget_set_visible(spacer_revealer, FALSE);
    island->spacer_revealer = spacer_revealer;
    
    GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_size_request(spacer, 0, 1);
    gtk_revealer_set_child(GTK_REVEALER(spacer_revealer), spacer);
    island->spacer = spacer;
    
    // === TIME LABEL ===
    GtkWidget *time_label = gtk_label_new("14:45");
    gtk_widget_set_name(time_label, "time");
    gtk_widget_set_valign(time_label, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_start(time_label, 4);
    gtk_widget_set_margin_end(time_label, 4);
    island->time_label = time_label;
    
    // === RIGHT ARROW ===
    GtkWidget *right_arrow_revealer = gtk_revealer_new();
    gtk_revealer_set_transition_type(GTK_REVEALER(right_arrow_revealer), 
                                      GTK_REVEALER_TRANSITION_TYPE_SLIDE_LEFT);
    gtk_revealer_set_transition_duration(GTK_REVEALER(right_arrow_revealer), 250);
    gtk_revealer_set_reveal_child(GTK_REVEALER(right_arrow_revealer), FALSE);
    island->right_arrow_revealer = right_arrow_revealer;
    
    GtkWidget *right_arrow = gtk_label_new("▸");
    gtk_widget_set_name(right_arrow, "arrow");
    gtk_widget_set_valign(right_arrow, GTK_ALIGN_CENTER);
    gtk_revealer_set_child(GTK_REVEALER(right_arrow_revealer), right_arrow);
    island->right_arrow = right_arrow;
    
    GtkGesture *right_click = gtk_gesture_click_new();
    g_signal_connect(right_click, "pressed", G_CALLBACK(on_right_arrow_clicked), island);
    gtk_widget_add_controller(right_arrow, GTK_EVENT_CONTROLLER(right_click));
    
    // === ASSEMBLE MAIN BOX ===
    gtk_box_append(GTK_BOX(main_box), left_arrow_revealer);
    for (int i = 0; i < island->num_applets; i++) {
        gtk_box_append(GTK_BOX(main_box), island->applet_revealers[i]);
    }
    gtk_box_append(GTK_BOX(main_box), spacer_revealer);
    gtk_box_append(GTK_BOX(main_box), time_label);
    gtk_box_append(GTK_BOX(main_box), right_arrow_revealer);
    
    // === EXPANDED CONTENT ===
    GtkWidget *expanded_revealer = gtk_revealer_new();
    gtk_revealer_set_transition_type(GTK_REVEALER(expanded_revealer), 
                                      GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
    gtk_revealer_set_transition_duration(GTK_REVEALER(expanded_revealer), 300);
    gtk_revealer_set_reveal_child(GTK_REVEALER(expanded_revealer), FALSE);
    island->expanded_revealer = expanded_revealer;
    
    GtkWidget *expanded_content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_name(expanded_content, "expanded-content");
    gtk_widget_set_halign(expanded_content, GTK_ALIGN_FILL);
    gtk_widget_set_opacity(expanded_content, 1.0);
    gtk_widget_set_size_request(expanded_content, 0, 0);
    gtk_widget_set_visible(expanded_content, FALSE);
    island->expanded_content = expanded_content;
    
    GtkWidget *expanded_label = gtk_label_new("");
    gtk_widget_set_name(expanded_label, "expanded-label");
    gtk_widget_set_halign(expanded_label, GTK_ALIGN_START);
    gtk_widget_set_valign(expanded_label, GTK_ALIGN_START);
    gtk_widget_set_margin_start(expanded_label, 10);
    gtk_widget_set_margin_end(expanded_label, 10);
    gtk_widget_set_margin_top(expanded_label, 8);
    gtk_widget_set_margin_bottom(expanded_label, 8);
    gtk_box_append(GTK_BOX(expanded_content), expanded_label);
    island->expanded_label = expanded_label;
    
    gtk_revealer_set_child(GTK_REVEALER(expanded_revealer), expanded_content);
    
    // === ASSEMBLE PILL ===
    gtk_box_append(GTK_BOX(pill), main_box);
    gtk_box_append(GTK_BOX(pill), expanded_revealer);
    gtk_box_append(GTK_BOX(main_container), pill);
    gtk_window_set_child(GTK_WINDOW(window), main_container);
    
    // === CSS ===
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(provider,
        "window { background: transparent; }"
        "#pill { background-color: rgba(0, 0, 0, 1); border-radius: 15px; padding: 3px; min-height: 24px; }"
        "#time { color: white; font-size: 13px; font-weight: bold; font-family: monospace; min-width: 42px; }"
        "#arrow { color: rgba(255, 255, 255, 1); font-size: 11px; min-width: 14px; }"
        "#arrow:hover { color: rgba(255, 255, 255, 0.7); }"
        "#applet { color: white; font-size: 14px; background: rgba(255, 255, 255, 0); border-radius: 12px; }"
        "#applet:hover { background: rgba(255, 255, 255, 0.2); }"
        "#expanded-content { background: transparent; padding: 0px; margin: 0px; }"
        "#expanded-label { color: white; font-size: 12px; }"
    );
    
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    
    // === HOVER EVENTS ===
    GtkEventController *motion_controller = gtk_event_controller_motion_new();
    g_signal_connect(motion_controller, "enter", G_CALLBACK(on_hover_enter), island);
    g_signal_connect(motion_controller, "leave", G_CALLBACK(on_hover_leave), island);
    gtk_widget_add_controller(pill, motion_controller);
    
    // === INITIAL DISPLAY ===
    update_applet_display(island);
    
    return island;
}

void island_destroy(DynamicIsland *island) {
    if (island->collapse_timer > 0) {
        g_source_remove(island->collapse_timer);
    }
    if (island->fade_timer > 0) {
        g_source_remove(island->fade_timer);
    }
    g_free(island);
}

// ===== ПУБЛИЧНОЕ API ДЛЯ МОДУЛЕЙ =====

void island_add_applet(DynamicIsland *island, const char *icon, const char *name) {
    if (island->num_applets >= MAX_APPLETS) return;
    
    int idx = island->num_applets;
    island->applets[idx].id = idx;
    island->applets[idx].icon = icon;
    island->applets[idx].name = name;
    island->num_applets++;
    
    // Создаём виджет для нового апплета
    island->applet_revealers[idx] = gtk_revealer_new();
    gtk_revealer_set_transition_type(GTK_REVEALER(island->applet_revealers[idx]), 
                                      GTK_REVEALER_TRANSITION_TYPE_SLIDE_RIGHT);
    gtk_revealer_set_transition_duration(GTK_REVEALER(island->applet_revealers[idx]), 200);
    gtk_revealer_set_reveal_child(GTK_REVEALER(island->applet_revealers[idx]), FALSE);
    
    island->applet_labels[idx] = gtk_label_new(icon);
    gtk_widget_set_name(island->applet_labels[idx], "applet");
    gtk_widget_set_valign(island->applet_labels[idx], GTK_ALIGN_CENTER);
    gtk_widget_set_size_request(island->applet_labels[idx], APPLET_SIZE, APPLET_SIZE);
    
    gtk_revealer_set_child(GTK_REVEALER(island->applet_revealers[idx]), 
                           island->applet_labels[idx]);
    
    island->applet_clicks[idx] = gtk_gesture_click_new();
    g_signal_connect(island->applet_clicks[idx], "pressed", 
                     G_CALLBACK(on_applet_clicked), island);
    gtk_widget_add_controller(island->applet_labels[idx], 
                              GTK_EVENT_CONTROLLER(island->applet_clicks[idx]));
    
    // Вставляем перед spacer
    gtk_box_insert_child_after(GTK_BOX(island->main_box), 
                               island->applet_revealers[idx],
                               island->spacer_revealer);
    
    update_applet_display(island);
}

void island_remove_applet(DynamicIsland *island, int index) {
    if (index < 0 || index >= island->num_applets) return;
    
    gtk_box_remove(GTK_BOX(island->main_box), island->applet_revealers[index]);
    island->applets[index].id = 0;
    
    // Сдвигаем оставшиеся
    for (int i = index; i < island->num_applets - 1; i++) {
        island->applets[i] = island->applets[i + 1];
        island->applet_revealers[i] = island->applet_revealers[i + 1];
        island->applet_labels[i] = island->applet_labels[i + 1];
        island->applet_clicks[i] = island->applet_clicks[i + 1];
    }
    island->num_applets--;
    
    if (island->current_applet >= island->num_applets) {
        island->current_applet = island->num_applets - 1;
    }
    update_applet_display(island);
}

void island_set_time(DynamicIsland *island, const char *time_str) {
    gtk_label_set_text(GTK_LABEL(island->time_label), time_str);
}
