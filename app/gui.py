import ctypes
import os
import tkinter as tk
from tkinter import filedialog, messagebox, simpledialog, ttk

# === Load C client shared library ===
lib = ctypes.CDLL(os.path.abspath("bin/client.so"))


class Client(ctypes.Structure):
    """C struct wrapper for client connection"""
    _fields_ = [
        ("sockfd", ctypes.c_int),
        ("server_addr", ctypes.c_byte * 16),
        ("is_connected", ctypes.c_int)
    ]


# Initialize client instance
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


# === GUI Application ===
class LocalBinApp(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("LocalBin Client")
        self.geometry("600x500")
        self.configure(bg="#1e1e1e")
        self.resizable(False, False)
        
        self._configure_styles()
        self._build_ui()

    def _configure_styles(self):
        """Configure ttk widget styles"""
        self.style = ttk.Style()
        self.style.configure("TButton", font=("Segoe UI", 11, "bold"), padding=10)
        self.style.configure("TLabel", foreground="#e0e0e0", background="#1e1e1e", font=("Segoe UI", 13))
        self.style.configure("TEntry", font=("Segoe UI", 12), padding=5)

    def _build_ui(self):
        """Build the complete user interface"""
        # Title with more spacing
        title_label = tk.Label(
            self,
            text="LocalBin File Sharing",
            fg="#00d9ff",
            bg="#1e1e1e",
            font=("Segoe UI", 50, "bold")
        )
        title_label.pack(pady=20)

        # Connection form
        self._build_connection_form()
        
        # Action buttons
        self._build_button_panel()
        
        # Log output
        self._build_log_panel()

    def _build_connection_form(self):
        """Build the server connection form"""
        form = tk.Frame(self, bg="#1e1e1e")
        form.pack(pady=15)

        self.host_entry = self._create_entry(form, "Server Host:", "127.0.0.1")
        self.port_entry = self._create_entry(form, "Port:", "8080")
        self.user_entry = self._create_entry(form, "Username:", "testuser")
        self.pass_entry = self._create_entry(form, "Password:", "password", show="*")

    def _build_button_panel(self):
        """Build the action button panel"""
        btn_frame = tk.Frame(self, bg="#1e1e1e")
        btn_frame.pack(pady=15, fill=tk.X, padx=30)

        # Configure equal column weights for uniform button sizing
        for i in range(4):
            btn_frame.columnconfigure(i, weight=1)

        # Create buttons with better spacing
        self.connect_btn = ttk.Button(btn_frame, text="Connect", command=self.connect)
        self.upload_btn = ttk.Button(btn_frame, text="Upload File", command=self.upload, state=tk.DISABLED)
        self.download_btn = ttk.Button(btn_frame, text="Download File", command=self.download, state=tk.DISABLED)
        self.disconnect_btn = ttk.Button(btn_frame, text="Disconnect", command=self.disconnect, state=tk.DISABLED)

        # Grid layout - all buttons in one row with better padding
        self.connect_btn.grid(row=0, column=0, padx=8, sticky="ew")
        self.upload_btn.grid(row=0, column=1, padx=8, sticky="ew")
        self.download_btn.grid(row=0, column=2, padx=8, sticky="ew")
        self.disconnect_btn.grid(row=0, column=3, padx=8, sticky="ew")

    def _build_log_panel(self):
        """Build the log output text area"""
        log_frame = tk.Frame(self, bg="#1e1e1e")
        log_frame.pack(pady=15, padx=30, fill=tk.BOTH, expand=True)
        
        # Add a label for the log section
        log_label = tk.Label(
            log_frame,
            text="Activity Log",
            fg="#00d9ff",
            bg="#1e1e1e",
            font=("Segoe UI", 13, "bold"),
            anchor="w"
        )
        log_label.pack(fill=tk.X, pady=(0, 8))

        self.log_box = tk.Text(
            log_frame,
            height=10,
            bg="#2d2d2d",
            fg="#b0f566",
            font=("Consolas", 11),
            wrap=tk.WORD,
            relief=tk.FLAT,
            padx=10,
            pady=10,
            insertbackground="#00d9ff"
        )
        self.log_box.pack(fill=tk.BOTH, expand=True)

    def _create_entry(self, parent, label_text, default="", show=None):
        """Create a labeled entry field"""
        frame = tk.Frame(parent, bg="#1e1e1e")
        frame.pack(pady=6)
        
        label = tk.Label(
            frame,
            text=label_text,
            width=14,
            anchor="e",
            fg="#e0e0e0",
            bg="#1e1e1e",
            font=("Segoe UI", 13)
        )
        label.pack(side=tk.LEFT, padx=8)
        
        entry = ttk.Entry(frame, show=show, width=28, font=("Segoe UI", 12))
        entry.insert(0, default)
        entry.pack(side=tk.LEFT)
        return entry
