#ifndef ISLAND_H
#define ISLAND_H

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

// ===== ПУБЛИЧНОЕ API =====
DynamicIsland* island_create(GtkApplication *app);
void island_destroy(DynamicIsland *island);
void island_add_applet(DynamicIsland *island, const char *icon, const char *name);
void island_remove_applet(DynamicIsland *island, int index);
void island_set_time(DynamicIsland *island, const char *time_str);

// ===== ВНУТРЕННИЕ ФУНКЦИИ (только для island.c) =====
// Они НЕ объявлены в .h, т.к. используются только внутри island.c
// Все они должны быть static в island.c

#endif
