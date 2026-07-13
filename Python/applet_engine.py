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
        # 1. System Icon Name or standard image file
        if style_type == "icon":
            if source.endswith(".png") or source.endswith(".jpg"):
                return Gtk.Image.new_from_file(source)
            else:
                # Fallback to system desktop theme icons (e.g., 'battery-good-symbolic')
                return Gtk.Image.new_from_icon_name(source)
                
        # 2. Raw SVG file (Handles vector rendering nicely)
        elif style_type == "svg":
            if os.path.exists(source):
                return Gtk.Image.new_from_file(source)
            print(f"Error: SVG file not found at {source}")
            return Gtk.Image.new_from_icon_name("image-missing")

        # 3. Animated GIF (Replaces the 'video' concept flawlessly without high resource use)
        elif style_type == "gif":
            try:
                # GTK 4 uses GdkPixbufAnimation to handle multi-frame images natively
                animation = GdkPixbuf.PixbufAnimation.new_from_file(source)
                image_widget = Gtk.Image()
                image_widget.set_from_animation(animation)
                return image_widget
            except Exception as e:
                print(f"Failed to load animated GIF: {e}")
                return Gtk.Image.new_from_icon_name("video-x-generic-symbolic")

        # 4. Standard Text Label (like a clock string or temperature number)
        elif style_type == "label":
            lbl = Gtk.Label(label=source)
            lbl.add_css_class("island-compact-text")
            # lbl.set_valign(Gtk.Align.BASELINE)
            # lbl.set_halign(Gtk.Align.CENTER)
            return lbl

        # Fallback default
        return Gtk.Box()


class GenericApplet:
    def __init__(self, json_path):
        with open(json_path, 'r') as f:
            self.config = json.load(f)

        self.name = self.config["name"]
        self.script = self.config.get("execute")
        
        self.compact_widget = WidgetFactory.create_compact_widget(
            self.config["compact"]["type"], 
            self.config["compact"]["source"]
        )
        
        self.expanded_widget = self.build_expanded_menu(self.config["expanded"])

        # Inject custom CSS styles if specified
        custom_css = self.config.get("style")
        if custom_css:
            self.apply_custom_styles(custom_css)

        if self.script and self.config.get("update_rate_ms"):
            GLib.timeout_add(self.config["update_rate_ms"], self.run_backend)

    def apply_custom_styles(self, css_string):
        provider = Gtk.CssProvider()
        provider.load_from_data(css_string.encode('utf-8'))
        
        # Apply CSS natively to both sub-widgets
        self.compact_widget.get_style_context().add_provider(
            provider, Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION
        )
        self.expanded_widget.get_style_context().add_provider(
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

    def build_expanded_menu(self, expanded_cfg):
        """Entry point for building the expanded view layout."""
        # Treat the root definition as a vertical box container by default if not specified
        if "type" not in expanded_cfg:
            expanded_cfg = {
                "type": "box",
                "orientation": "vertical",
                "items": expanded_cfg.get("items", [])
            }
        return self._parse_layout_node(expanded_cfg)

    def _parse_layout_node(self, node):
        """Recursively parses layout configurations into native GTK 4 components."""
        node_type = node.get("type", "text")

        # --- CONTAINERS ---
        if node_type == "box":
            is_vert = node.get("orientation", "vertical") == "vertical"
            box = Gtk.Box(
                orientation=Gtk.Orientation.VERTICAL if is_vert else Gtk.Orientation.HORIZONTAL,
                spacing=node.get("spacing", 8)
            )
            if "name" in node:
                box.set_name(node["name"])
            
            # Recursively append child components
            for item in node.get("items", []):
                child_widget = self._parse_layout_node(item)
                box.append(child_widget)
            return box

        # --- BASIC WIDGETS ---
        elif node_type == "text":
            lbl = Gtk.Label(label=node.get("label", ""))
            lbl.set_halign(Gtk.Align.START if node.get("align", "start") == "start" else Gtk.Align.CENTER)
            if node.get("bold", False):
                lbl.add_css_class("island-bold-text")
            return lbl

        elif node_type == "button":
            btn = Gtk.Button(label=node.get("label", ""))
            btn.add_css_class("island-menu-btn")
            if "command" in node:
                # Using a default argument inside lambda to safely trap the unique command string
                btn.connect("clicked", lambda b, cmd=node["command"]: subprocess.Popen(cmd, shell=True))
            return btn

        # --- ADVANCED INTERACTIVE COMPONENTS ---
        elif node_type == "input":
            # Text Entry Field (e.g., search or entry payloads)
            entry = Gtk.Entry()
            entry.set_placeholder_text(node.get("placeholder", ""))
            entry.set_hexpand(True)
            if "command" in node:
                # Fires the shell command when pressing Enter, passing the text input field string
                # We replace '{}' in the command string with the actual input text
                entry.connect("activate", lambda e, cmd=node["command"]: subprocess.Popen(
                    cmd.replace("{}", e.get_buffer().get_text()), shell=True
                ))
            return entry

        elif node_type == "slider":
            # Adjust ranges like Volume, Brightness, etc.
            # Arguments: orientation, min, max, step
            adj = Gtk.Adjustment(value=node.get("value", 50), lower=node.get("min", 0), upper=node.get("max", 100), step_increment=1)
            scale = Gtk.Scale(orientation=Gtk.Orientation.HORIZONTAL, adjustment=adj)
            scale.set_hexpand(True)
            scale.set_draw_value(False) # Hides standard floating text value above slider
            
            if "command" in node:
                # Fires when user adjusts slider thumb
                # Replaces '{}' with the numerical floating value
                scale.connect("value-changed", lambda s, cmd=node["command"]: subprocess.Popen(
                    cmd.replace("{}", str(int(s.get_value()))), shell=True
                ))
            return scale

        elif node_type == "toggle":
            # On/Off Switch component (e.g., Bluetooth, Wi-Fi toggles)
            switch_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=10)
            lbl = Gtk.Label(label=node.get("label", ""))
            switch = Gtk.Switch(active=node.get("active", False))
            switch.set_halign(Gtk.Align.END)
            switch_box.set_hexpand(True)
            lbl.set_hexpand(True)
            lbl.set_halign(Gtk.Align.START)
            
            switch_box.append(lbl)
            switch_box.append(switch)
            
            if "command" in node:
                # Replaces '{}' with either 'true' or 'false' depending on trigger state
                switch.connect("state-set", lambda s, state, cmd=node["command"]: [
                    subprocess.Popen(cmd.replace("{}", str(state).lower()), shell=True),
                    False # Returning False propagates mutation safely in GTK
                ][1])
            return switch_box

        return Gtk.Box()