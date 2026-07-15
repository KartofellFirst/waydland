import os
import json
import subprocess
from gi.repository import Gtk, GdkPixbuf, GLib

class WidgetFactory:
    @staticmethod
    def create_compact_widget(style_type, source):
        """
        Dynamically returns the correct GTK widget based on configuration.
        Supports: system icons, custom SVGs, animated GIFs, or plain text labels.
        """
        if style_type == "icon":
            if source.endswith(".png") or source.endswith(".jpg"):
                return Gtk.Image.new_from_file(source), f"<img src='{source}'>"
            return Gtk.Box(), "<div style='color: red;'>unsupported image format</div>"
                
        # 2. Raw SVG file (Handles vector rendering nicely)
        elif style_type == "svg":
            if os.path.exists(source):
                return Gtk.Image.new_from_file(source), f"<svg path='{source}'>"
            print(f"Error: SVG file not found at {source}")
            return Gtk.Box(), "<div style='color: red;'>error loading image</div>"

        # 3. Animated GIF (Replaces the 'video' concept flawlessly without high resource use)
        elif style_type == "gif":
            try:
                # GTK 4 uses GdkPixbufAnimation to handle multi-frame images natively
                animation = GdkPixbuf.PixbufAnimation.new_from_file(source)
                image_widget = Gtk.Image()
                image_widget.set_from_animation(animation)
                return image_widget, f"<img src='{source}'>"
            except Exception as e:
                print(f"Failed to load animated GIF: {e}")
                return Gtk.Box(), "<div style='color: red;'>error loading gif</div>"

        # 4. Standard Text Label (like a clock string or temperature number)
        elif style_type == "label":
            lbl = Gtk.Label(label=source)
            lbl.add_css_class("island-compact-text")
            return lbl, f"<span>{source}</span>"

        # Fallback default
        return Gtk.Box(), "<div></div>"


class GenericApplet:
    def __init__(self, json_path):
        with open(json_path, 'r') as f:
            self.config = json.load(f)

        self.name = self.config["name"]
        self.script = self.config.get("execute")
        
        self.compact_widget, self.html_tag = WidgetFactory.create_compact_widget(
            self.config["compact"]["type"], 
            self.config["compact"]["source"]
        )
        
        self.expanded = self.config.get("expanded") or f"""
            <html style="background-color: black; display: flex; justify-content: center;">{self.html_tag}</html>
        """

        # Inject custom CSS styles if specified
        custom_css = self.config.get("style")
        if custom_css:
            self.apply_custom_styles(custom_css)

        if self.script and self.config.get("update_rate_ms"):
            GLib.timeout_add(self.config["update_rate_ms"], self.run_backend)

    def apply_custom_styles(self, css_string):
        provider = Gtk.CssProvider()
        provider.load_from_data(css_string.encode('utf-8'))
        
        # Apply CSS natively to the mini icon
        self.compact_widget.get_style_context().add_provider(
            provider, Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION
        )

    def run_backend(self):
        def _execute():
            try:
                output = subprocess.check_output(self.script, shell=True, text=True).strip()
                GLib.idle_add(self.handle_backend_response, output)
            except Exception as e:
                print(f"[{self.name}] Script execution failure: {e}")
                
        import threading
        threading.Thread(target=_execute, daemon=True).start()
        return True

    def handle_backend_response(self, raw_data):
        if self.config["compact"]["type"] == "label":
            self.compact_widget.set_text(raw_data)
