<h1>Waydland</h1>
<h5>A Wayland dynamic island component with a mission to replace waybar-like menus in the distros with hyprland-looking wm philosophy to expand the concept of <i>no-mouse-use</i>. <i>Built with gtk4.0 on pure C</i></h5>
<br><br>
<h3>CURRENTLY IN WORK</h3>
<hr>
<h4>To run it on your device, you have to build it first.</h4> <b>Here is how to do that:</b>
<br><br>
  1. Download the main.c file<br><br>
  2. Open your terminal in the directory where it's located <br><br>
      <code>cd ~/Downloads</code> <br><br>
  3. Build the file using gcc <br><br>
      <code>gcc -o waydland main.c $(pkg-config --cflags --libs gtk4 gtk4-layer-shell-0)</code><br><br>
  4. If the file is somehow not executable, give it the rights with <code>chmod +x</code> command <br><br>
  5. Now you can set it to autoload with <code>exec-once = /path/to/waydland</code> in your hyprland config or just run it through your terminal <br><br>
      <code>./waydland</code><br><br>
