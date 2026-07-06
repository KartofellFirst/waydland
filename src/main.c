#include <gtk/gtk.h>
#include "island.h"

static void activate(GtkApplication *app, gpointer user_data) {
    (void)user_data; // РїРѕРґР°РІР»СЏРµРј РїСЂРµРґСѓРїСЂРµР¶РґРµРЅРёРµ
    DynamicIsland *island = island_create(app);
    gtk_window_present(GTK_WINDOW(island->window));
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new(
        "com.github.waydland", 
        G_APPLICATION_DEFAULT_FLAGS
    );
    
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    
    return status;
}
