"""Small local import window, sharing the command-line import pipeline."""
from pathlib import Path
import queue
import re
import subprocess
import sys
import threading
import tkinter as tk
from tkinter import filedialog, messagebox, ttk

from project_paths import ROOT


class ImportWindow:
    def __init__(self, root):
        self.root = root
        self.root.title("Rise 2 — Import your game")
        self.root.geometry("850x640")
        self.sources = []
        self.messages = queue.Queue()
        self.running = False
        self.profile = tk.StringVar(value="my-game")
        self.music = tk.StringVar()
        self.mode = tk.StringVar(value="auto")
        frame = ttk.Frame(root, padding=18)
        frame.pack(fill="both", expand=True)
        ttk.Label(frame, text="Rise 2 — import your original game", font=("Arial", 17, "bold")).pack(anchor="w")
        ttk.Label(frame, text="Source and converted files stay on your computer in LOCAL/.\nDirector's Cut only needs Disc 1 for the game and music.").pack(anchor="w", pady=10)
        self.listbox = tk.Listbox(frame, height=5)
        self.listbox.pack(fill="x")
        bar = ttk.Frame(frame)
        bar.pack(fill="x", pady=5)
        self.buttons = []
        for label, command in (("Add folder / CD", self.add_folder), ("Add ISO, CUE, or ZIP", self.add_files), ("Remove", self.remove)):
            button = ttk.Button(bar, text=label, command=command)
            button.pack(side="left", padx=(0, 6))
            self.buttons.append(button)
        row = ttk.Frame(frame)
        row.pack(fill="x", pady=8)
        ttk.Label(row, text="Separate music (optional)").pack(side="left")
        ttk.Entry(row, textvariable=self.music).pack(side="left", fill="x", expand=True, padx=8)
        ttk.Button(row, text="Browse…", command=lambda: self.music.set(filedialog.askdirectory() or self.music.get())).pack(side="left")
        row = ttk.Frame(frame)
        row.pack(fill="x", pady=8)
        ttk.Label(row, text="Local profile").pack(side="left")
        ttk.Entry(row, textvariable=self.profile, width=24).pack(side="left", padx=8)
        ttk.Label(row, text="Music mode").pack(side="left", padx=(15, 8))
        ttk.Combobox(row, textvariable=self.mode, values=("auto", "cd", "digital", "effects", "off"), state="readonly", width=12).pack(side="left")
        ttk.Label(frame, text="Auto: CD tracks → digital MRS/MRW music → ambience → silence.\nThe port plays imported CD tracks; MRS digital music is preserved but not yet implemented.").pack(anchor="w")
        self.start_button = ttk.Button(frame, text="Import and prepare assets", command=self.start)
        self.start_button.pack(anchor="w", pady=12)
        self.log = tk.Text(frame, height=14, wrap="word", state="disabled")
        self.log.pack(fill="both", expand=True)
        self.root.protocol("WM_DELETE_WINDOW", self.close)
        self.root.after(100, self.poll)

    def add_folder(self):
        path = filedialog.askdirectory(title="Game data folder or CD root")
        if path:
            self.add(path)

    def add_files(self):
        for path in filedialog.askopenfilenames(filetypes=[("Disc images and archives", "*.iso *.cue *.bin *.zip")]):
            self.add(path)

    def add(self, path):
        if path not in self.sources:
            self.sources.append(path)
            self.listbox.insert("end", path)

    def remove(self):
        for index in reversed(self.listbox.curselection()):
            del self.sources[index]
            self.listbox.delete(index)

    def start(self):
        if self.running:
            return
        if not self.sources or not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_-]{0,47}", self.profile.get()):
            messagebox.showerror("Import", "Add a source and use a simple profile name (letters, numbers, hyphens).")
            return
        args = [sys.executable, "-u", str(ROOT / "TOOLS/import_game.py"), "--source", *self.sources,
                "--output", str(ROOT / "LOCAL" / self.profile.get()), "--music-mode", self.mode.get()]
        if self.music.get():
            args += ["--music", self.music.get()]
        self.running = True
        self.start_button.configure(state="disabled")
        for button in self.buttons:
            button.configure(state="disabled")
        threading.Thread(target=self.worker, args=(args,), daemon=True).start()

    def worker(self, args):
        try:
            kwargs = {"creationflags": subprocess.CREATE_NO_WINDOW} if sys.platform == "win32" else {}
            with subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                                  encoding="utf-8", errors="replace", env={**__import__('os').environ, "PYTHONUTF8": "1"}, **kwargs) as process:
                for line in process.stdout:
                    self.messages.put(line)
                self.messages.put(("done", process.wait()))
        except Exception as exc:
            self.messages.put(str(exc) + "\n")
            self.messages.put(("done", 1))

    def poll(self):
        while not self.messages.empty():
            item = self.messages.get_nowait()
            if isinstance(item, tuple):
                self.running = False
                self.start_button.configure(state="normal")
                for button in self.buttons:
                    button.configure(state="normal")
                item = "\nImport complete. See import-report.json in your profile.\n" if item[1] == 0 else "\nImport stopped. See the messages above.\n"
            self.log.configure(state="normal")
            self.log.insert("end", item)
            self.log.see("end")
            self.log.configure(state="disabled")
        self.root.after(100, self.poll)

    def close(self):
        if self.running:
            messagebox.showinfo("Import in progress", "Wait for this import to finish before closing the window.")
        else:
            self.root.destroy()


if __name__ == "__main__":
    app = tk.Tk()
    ImportWindow(app)
    app.mainloop()
