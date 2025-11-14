import ctypes
import os
import tkinter as tk
from tkinter import filedialog, messagebox, simpledialog, ttk

# === Load C client shared library ===
lib = ctypes.CDLL(os.path.abspath("bin/client.so"))

class Client(ctypes.Structure):
    _fields_ = [
        ("sockfd", ctypes.c_int),
        ("server_addr", ctypes.c_byte * 16),
        ("is_connected", ctypes.c_int)
    ]

client = Client()

# === Define C function signatures ===
lib.client_connect.argtypes = [ctypes.POINTER(Client), ctypes.c_char_p, ctypes.c_int]
lib.client_connect.restype = ctypes.c_int

lib.client_auth.argtypes = [ctypes.POINTER(Client), ctypes.c_char_p, ctypes.c_char_p]
lib.client_auth.restype = ctypes.c_int

lib.client_upload.argtypes = [ctypes.POINTER(Client), ctypes.c_char_p, ctypes.c_char_p]
lib.client_upload.restype = ctypes.c_int

lib.client_download.argtypes = [ctypes.POINTER(Client), ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p]
lib.client_download.restype = ctypes.c_int

lib.client_disconnect.argtypes = [ctypes.POINTER(Client)]
lib.client_disconnect.restype = None


# === GUI Class ===
class LocalBinApp(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("LocalBin Client")
        self.geometry("520x380")
        self.configure(bg="#181818")
        self.resizable(False, False)

        self.style = ttk.Style()
        self.style.configure("TButton", font=("Segoe UI", 10, "bold"), padding=6)
        ## UPDATED 1:32 — larger text to increase readibility
        self.style.configure("TLabel", foreground="white", background="#181818", font=("Segoe UI", 12))
        self.style.configure("TEntry", font=("Segoe UI", 11))


        self._build_ui()

    def _build_ui(self):
        ### UPDATED 1:32 — bigger blue title text
        tk.Label(
            self,
            text="LocalBin File Sharing Client",
            fg="#00ffcc",
            bg="#181818",
            font=("Segoe UI", 25, "bold")
        ).pack(pady=10)
        form = tk.Frame(self, bg="#181818")
        form.pack(pady=5)

        self.host_entry = self._entry(form, "Server Host:", "127.0.0.1")
        self.port_entry = self._entry(form, "Port:", "8080")
        self.user_entry = self._entry(form, "Username:", "testuser")
        self.pass_entry = self._entry(form, "Password:", "password", show="*")

        # Buttons frame
        btn_frame = tk.Frame(self, bg="#181818")
        btn_frame.pack(pady=10)

        self.connect_btn = ttk.Button(btn_frame, text="Connect", command=self.connect)
        self.upload_btn = ttk.Button(btn_frame, text="Upload File", command=self.upload, state=tk.DISABLED)
        self.download_btn = ttk.Button(btn_frame, text="Download File", command=self.download, state=tk.DISABLED)
        self.disconnect_btn = ttk.Button(btn_frame, text="Disconnect", command=self.disconnect, state=tk.DISABLED)

        self.connect_btn.grid(row=0, column=0, padx=10)
        self.upload_btn.grid(row=0, column=1, padx=10)
        self.download_btn.grid(row=0, column=2, padx=10)
        self.disconnect_btn.grid(row=0, column=3, padx=10)

        # Log output
        self.log_box = tk.Text(self, height=10, width=60, bg="#202020", fg="#00ff00",
                               font=("Consolas", 10), wrap=tk.WORD)
        self.log_box.pack(pady=10)

    def _entry(self, parent, label, default="", show=None):
        frame = tk.Frame(parent, bg="#181818")
        frame.pack(pady=2)
        tk.Label(frame, text=label, width=12, anchor="e").pack(side=tk.LEFT, padx=3)
        entry = ttk.Entry(frame, show=show, width=30)
        entry.insert(0, default)
        entry.pack(side=tk.LEFT)
        return entry

    def log(self, msg):
        self.log_box.insert(tk.END, msg + "\n")
        self.log_box.see(tk.END)

    def connect(self):
        host = self.host_entry.get().encode()
        port = int(self.port_entry.get())
        if lib.client_connect(ctypes.byref(client), host, port) == 0:
            self.log("[INFO] Connected to server.")
            user = self.user_entry.get().encode()
            pw = self.pass_entry.get().encode()
            if lib.client_auth(ctypes.byref(client), user, pw) == 0:
                self.log(f"[INFO] Authenticated as {self.user_entry.get()}.")
                self.upload_btn.config(state=tk.NORMAL)
                self.download_btn.config(state=tk.NORMAL)
                self.disconnect_btn.config(state=tk.NORMAL)
            else:
                self.log("[ERROR] Authentication failed.")
                messagebox.showerror("Error", "Authentication failed.")
        else:
            self.log("[ERROR] Could not connect to server.")
            messagebox.showerror("Error", "Failed to connect to server.")

    def upload(self):
        filepath = filedialog.askopenfilename(title="Select file to upload")
        if not filepath:
            return
        user = self.user_entry.get().encode()
        if lib.client_upload(ctypes.byref(client), user, filepath.encode()) == 0:
            self.log(f"[INFO] Uploaded: {os.path.basename(filepath)}")
            messagebox.showinfo("Success", f"Uploaded {os.path.basename(filepath)}")
        else:
            self.log("[ERROR] Upload failed.")
            messagebox.showerror("Error", "File upload failed.")

    def download(self):
        filename = simpledialog.askstring("Download File", "Enter filename to download:")
        if not filename:
            return
        save_dir = filedialog.askdirectory(title="Select folder to save file")
        if not save_dir:
            return
        user = self.user_entry.get().encode()
        if lib.client_download(ctypes.byref(client), user, filename.encode(), save_dir.encode()) == 0:
            self.log(f"[INFO] Downloaded: {filename}")
            messagebox.showinfo("Success", f"Downloaded {filename}")
        else:
            self.log("[ERROR] Download failed.")
            messagebox.showerror("Error", "Download failed. File may not exist on server.")

    ### UPDATED — Adds confirmation
    def disconnect(self):
        if not messagebox.askyesno("Confirm Disconnect", "Are you sure you want to disconnect?"):
            return
        lib.client_disconnect(ctypes.byref(client))
        self.log("[INFO] Disconnected from server.")
        self.upload_btn.config(state=tk.DISABLED)
        self.download_btn.config(state=tk.DISABLED)
        self.disconnect_btn.config(state=tk.DISABLED)
        messagebox.showinfo("LocalBin", "Disconnected from server.")


if __name__ == "__main__":
    app = LocalBinApp()
    app.mainloop()
