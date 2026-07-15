"""IDK how good or not that code is, this watcher is vibecoded"""

import os
import asyncio
import threading
from typing import Callable, Any
from pathlib import Path

class ConfigWatcher:
    """
    Наблюдатель за изменениями конфигурационного файла с использованием inotify.
    Интегрируется с главным циклом GTK через GLib.idle_add.
    """
    def __init__(self, config_path: str, callback: Callable[[], Any]):
        """
        Args:
            config_path: путь к отслеживаемому файлу
            callback: функция, вызываемая при изменении файла
        """
        self.config_path = os.path.abspath(config_path)
        self.callback = callback
        self._watch_thread = None
        self._stop_event = threading.Event()
        self._last_modified = 0
        self._check_interval = 0.5  # секунды между проверками
        
    def start(self):
        """Запускает мониторинг файла в отдельном потоке."""
        if not os.path.exists(self.config_path):
            print(f"Config file not found: {self.config_path}")
            return False
            
        try:
            self._stop_event.clear()
            self._watch_thread = threading.Thread(
                target=self._watch_loop, 
                daemon=True
            )
            self._watch_thread.start()
            print(f"Started watching: {self.config_path}")
            return True
        except Exception as e:
            print(f"Failed to start config watcher: {e}")
            return False
            
    def _watch_loop(self):
        """Основной цикл отслеживания изменений файла."""
        import gi
        gi.require_version('GLib', '2.0')
        from gi.repository import GLib
        
        while not self._stop_event.is_set():
            try:
                if os.path.exists(self.config_path):
                    current_modified = os.path.getmtime(self.config_path)
                    
                    if current_modified != self._last_modified:
                        self._last_modified = current_modified
                        # Вызываем callback в главном потоке GTK
                        GLib.idle_add(self._safe_callback)
                        
                self._stop_event.wait(self._check_interval)
            except Exception as e:
                print(f"Error watching config: {e}")
                self._stop_event.wait(1)
    
    def _safe_callback(self):
        """Безопасный вызов callback в главном потоке."""
        try:
            if self.callback:
                self.callback()
        except Exception as e:
            print(f"Error in config callback: {e}")
        return False  # Прекращаем idle callback
        
    def stop(self):
        """Останавливает мониторинг."""
        self._stop_event.set()
        if self._watch_thread and self._watch_thread.is_alive():
            self._watch_thread.join(timeout=2)


class InotifyWatcher:
    """
    Более эффективный наблюдатель с использованием pyinotify (если доступен)
    или watchdog как запасной вариант.
    """
    def __init__(self, config_path: str, callback: Callable[[], Any]):
        self.config_path = os.path.abspath(config_path)
        self.callback = callback
        self._observer = None
        self._handler = None
        
    def start(self):
        """Запускает мониторинг с использованием наилучшего доступного метода."""
        # Пробуем watchdog
        if self._try_watchdog():
            return True
        # Пробуем pyinotify
        elif self._try_pyinotify():
            return True
        # Используем fallback polling метод
        else:
            print("Using polling fallback for config watching")
            self._simple_watcher = ConfigWatcher(self.config_path, self.callback)
            return self._simple_watcher.start()
    
    def _try_watchdog(self):
        """Пробует использовать библиотеку watchdog."""
        try:
            from watchdog.observers import Observer
            from watchdog.events import FileSystemEventHandler
            import gi
            gi.require_version('GLib', '2.0')
            from gi.repository import GLib
            
            class ConfigHandler(FileSystemEventHandler):
                def __init__(self, callback):
                    self.callback = callback
                    self._pending = False
                    
                def on_modified(self, event):
                    if event.src_path == os.path.abspath(self.callback.__self__.config_path if hasattr(self.callback, '__self__') else ''):
                        return
                    if not self._pending:
                        self._pending = True
                        # Debounce и вызов в главном потоке
                        def delayed_call():
                            self._pending = False
                            self.callback()
                            return False
                        GLib.timeout_add(500, delayed_call)
            
            watch_dir = os.path.dirname(self.config_path)
            self._handler = ConfigHandler(self.callback)
            self._observer = Observer()
            self._observer.schedule(self._handler, watch_dir, recursive=False)
            self._observer.start()
            print(f"Started watchdog observer for: {watch_dir}")
            return True
            
        except ImportError:
            return False
        except Exception as e:
            print(f"Watchdog failed: {e}")
            return False
    
    def _try_pyinotify(self):
        """Пробует использовать pyinotify."""
        try:
            import pyinotify
            import gi
            gi.require_version('GLib', '2.0')
            from gi.repository import GLib
            
            class EventHandler(pyinotify.ProcessEvent):
                def __init__(self, callback):
                    self.callback = callback
                    self._pending = False
                    
                def process_IN_MODIFY(self, event):
                    if not self._pending:
                        self._pending = True
                        GLib.timeout_add(500, self._delayed_callback)
                        
                def _delayed_callback(self):
                    self._pending = False
                    self.callback()
                    return False
            
            wm = pyinotify.WatchManager()
            mask = pyinotify.IN_MODIFY
            self._handler = EventHandler(self.callback)
            self._notifier = pyinotify.ThreadedNotifier(wm, self._handler)
            wm.add_watch(self.config_path, mask)
            self._notifier.start()
            return True
            
        except ImportError:
            return False
        except Exception as e:
            print(f"pyinotify failed: {e}")
            return False
    
    def stop(self):
        """Останавливает мониторинг."""
        if hasattr(self, '_observer') and self._observer:
            self._observer.stop()
            self._observer.join()
        if hasattr(self, '_notifier') and hasattr(self, '_notifier'):
            self._notifier.stop()
        if hasattr(self, '_simple_watcher'):
            self._simple_watcher.stop()


class ConfigReloader:
    """
    Управляет перезагрузкой конфигурации и уведомлением зависимых компонентов.
    Реализует паттерн Observer.
    """
    _instance = None
    
    def __new__(cls):
        if cls._instance is None:
            cls._instance = super().__new__(cls)
            cls._instance._initialized = False
        return cls._instance
    
    def __init__(self):
        if self._initialized:
            return
        self._initialized = True
        self._observers = []
        self._watcher = None
        self._reload_lock = False
        
    def add_observer(self, observer):
        """Добавляет наблюдателя за изменениями конфигурации."""
        if observer not in self._observers:
            self._observers.append(observer)
            print(f"Added config observer: {observer.__class__.__name__}")
            
    def remove_observer(self, observer):
        """Удаляет наблюдателя."""
        if observer in self._observers:
            self._observers.remove(observer)
            print(f"Removed config observer: {observer.__class__.__name__}")
            
    def start_watching(self, config_path: str = "waydland.conf"):
        """Запускает отслеживание файла конфигурации."""
        if self._watcher:
            self._watcher.stop()
            
        # Используем InotifyWatcher для автоматического выбора лучшего метода
        self._watcher = InotifyWatcher(config_path, self._on_config_changed)
        return self._watcher.start()
        
    def _on_config_changed(self):
        """Обработчик изменения конфигурации."""
        if self._reload_lock:
            return
            
        self._reload_lock = True
        try:
            print("Configuration file changed, reloading...")
            self._reload_style_module()
            
            # Уведомляем всех наблюдателей
            for observer in self._observers:
                try:
                    if hasattr(observer, 'on_config_reloaded'):
                        observer.on_config_reloaded()
                except Exception as e:
                    print(f"Error notifying observer {observer.__class__.__name__}: {e}")
        except Exception as e:
            print(f"Error during config reload: {e}")
        finally:
            self._reload_lock = False
            
    def _reload_style_module(self):
        """Перезагружает модуль style с новыми значениями."""
        try:
            import style
            import importlib
            importlib.reload(style)
            print("Style module reloaded successfully")
        except Exception as e:
            print(f"Failed to reload style module: {e}")
        
    def stop_watching(self):
        """Останавливает отслеживание."""
        if self._watcher:
            self._watcher.stop()
            self._watcher = None