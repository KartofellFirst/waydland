import gi
gi.require_version('Gtk', '4.0')
from gi.repository import Gtk, Gio, Gdk

from platform import get_platform
from style import load_css
from island import IslandWidget

class WaydlandApp(Gtk.Application):
    def __init__(self):
        super().__init__(
            application_id='com.kartofellfirst.waydland',
            flags=Gio.ApplicationFlags.FLAGS_NONE
        )
    
    def do_startup(self):
        Gtk.Application.do_startup(self)
        load_css()
        
        # Inject global transparency for the top-level window
        # This ensures the 600x400 phantom bounding box is completely invisible
        provider = Gtk.CssProvider()
        provider.load_from_string("window { background: rgba(0, 0, 0, 0.01); }")
        Gtk.StyleContext.add_provider_for_display(
            Gdk.Display.get_default(), 
            provider, 
            Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION
        )

    def do_activate(self):
        window = self.props.active_window
        if not window:
            window = Gtk.Window()
            window.set_application(self) 
            
            platform = get_platform()
            platform.setup(window)

            # The island handles its own hover/click logic internally
            island = IslandWidget()
            window.set_child(island)

        # THE PHANTOM WINDOW FIX:
        # We set a large, fixed bounding box. The OS window manager never resizes 
        # this top-level container, completely eliminating the Wayland layout jump.
        # GTK4's native input shape calculation will automatically let clicks pass 
        # through the empty space.
        # window.set_default_size(600, 400)
        # window.fullscreen()
        display = Gdk.Display.get_default()
        monitors = display.get_monitors()
        
        if monitors.get_n_items() > 0:
            primary_monitor = monitors.get_item(0)
            geometry = primary_monitor.get_geometry()
            
            # Match the display resolution bounds dynamically
            monitor_w = geometry.width
            monitor_h = geometry.height
        else:
            # Safe absolute fallbacks if display properties aren't warm yet
            monitor_w = 1920
            monitor_h = 1080

        # Set your high-ceiling canvas bounds without breaking macOS transparency
        window.set_default_size(monitor_w, monitor_h)
        window.set_resizable(False) 
        
        window.present()