# ================================
# LocalBin Build System (C only)
# ================================

CC       = clang
CFLAGS   = -Wall -pthread -Icore/common -Icore/server -Icore/client
LDFLAGS  = -pthread
BIN_DIR  = bin
DATA_DIR = data

COMMON_SRC = core/common/common.c core/common/protocol.c
SERVER_SRC = core/server/auth.c core/server/file_ops.c core/server/client_handler.c core/server/server.c
CLIENT_SRC = core/client/client.c

# === Default Target ===
all: $(BIN_DIR)/server $(BIN_DIR)/client

# ================================
# SERVER BUILD
# ================================
$(BIN_DIR)/server: $(COMMON_SRC) $(SERVER_SRC) core/server/main.c
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(COMMON_SRC) $(SERVER_SRC) core/server/main.c -o $(BIN_DIR)/server $(LDFLAGS)

# Shared library for Python FFI (server)
$(BIN_DIR)/server.so: $(COMMON_SRC) $(SERVER_SRC)
	@mkdir -p $(BIN_DIR)
	$(CC) -shared -fPIC $(CFLAGS) $(COMMON_SRC) $(SERVER_SRC) -o $(BIN_DIR)/server.so $(LDFLAGS)

# ================================
# CLIENT BUILD
# ================================
$(BIN_DIR)/client: $(COMMON_SRC) $(CLIENT_SRC) core/client/main.c
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(COMMON_SRC) $(CLIENT_SRC) core/client/main.c -o $(BIN_DIR)/client $(LDFLAGS)

# Shared library for Python GUI (FFI)
$(BIN_DIR)/client.so: $(COMMON_SRC) $(CLIENT_SRC)
	@mkdir -p $(BIN_DIR)
	$(CC) -shared -fPIC $(CFLAGS) $(COMMON_SRC) $(CLIENT_SRC) -o $(BIN_DIR)/client.so $(LDFLAGS)

# ================================
# TEST / RUN COMMANDS
# ================================

# Run the server on port 8080
run-server: $(BIN_DIR)/server
	@mkdir -p $(DATA_DIR)/logs $(DATA_DIR)/storage
	./$(BIN_DIR)/server 8080

# Run the server on custom port
run-server-port: $(BIN_DIR)/server
	@mkdir -p $(DATA_DIR)/logs $(DATA_DIR)/storage
	@read -p "Enter port number (default 8080): " port; \
	./$(BIN_DIR)/server $${port:-8080}

# Run the client (connects to localhost)
run-client: $(BIN_DIR)/client
	./$(BIN_DIR)/client

# Build both shared libs for Python integration
shared: $(BIN_DIR)/server.so $(BIN_DIR)/client.so

# Run Python GUI client
run-gui: $(BIN_DIR)/client.so
	@if [ ! -f app/gui.py ]; then \
		echo "Error: app/gui.py not found"; \
		exit 1; \
	fi
	cd app && python3 gui.py

# ================================
# NETWORK TESTING UTILITIES
# ================================

# Check if server port is open (requires netstat)
check-server:
	@echo "Checking for server on port 8080..."
	@netstat -tlnp 2>/dev/null | grep :8080 || echo "No server found on port 8080"

# Test connection to server (requires nc - netcat)
test-connection:
	@read -p "Enter server IP (default 127.0.0.1): " ip; \
	read -p "Enter port (default 8080): " port; \
	nc -zv $${ip:-127.0.0.1} $${port:-8080}

# Show server logs
show-logs:
	@if [ -d $(DATA_DIR)/logs ]; then \
		echo "=== Recent Server Logs ==="; \
		tail -n 50 $(DATA_DIR)/logs/server-*.log 2>/dev/null || echo "No logs found"; \
	else \
		echo "No logs directory found"; \
	fi

# ================================
# DEPLOYMENT HELPERS
# ================================

# Install system dependencies (Debian/Ubuntu)
install-deps:
	@echo "Installing build dependencies..."
	sudo apt-get update
	sudo apt-get install -y build-essential clang python3 python3-tk netcat-openbsd

# Setup firewall rule for port 8080 (Ubuntu/Debian with ufw)
setup-firewall:
	@echo "Opening port 8080 in firewall..."
	sudo ufw allow 8080/tcp
	sudo ufw status

# Get server's network information
network-info:
	@echo "=== Network Interfaces ==="
	@ip addr show | grep "inet " | grep -v "127.0.0.1"
	@echo ""
	@echo "=== Open Ports ==="
	@netstat -tlnp 2>/dev/null | grep LISTEN || ss -tlnp | grep LISTEN

# ================================
# CLEANUP
# ================================

clean:
	rm -rf $(BIN_DIR)

# Clean all build artifacts and data
clean-all: clean
	rm -rf $(DATA_DIR)/logs $(DATA_DIR)/storage

# Reset everything (keeps users.json)
reset:
	rm -rf $(BIN_DIR) $(DATA_DIR)/logs $(DATA_DIR)/storage
	@echo "Reset complete. User credentials preserved in $(DATA_DIR)/users.json"

# Complete wipe including user data
reset-hard:
	rm -rf $(BIN_DIR) $(DATA_DIR)
	@echo "Complete reset. All data removed."

# ================================
# BUILD INFORMATION
# ================================

info:
	@echo "=== LocalBin Build System ==="
	@echo "Compiler: $(CC)"
	@echo "C Flags: $(CFLAGS)"
	@echo "LD Flags: $(LDFLAGS)"
	@echo "Binary Dir: $(BIN_DIR)"
	@echo "Data Dir: $(DATA_DIR)"
	@echo ""
	@echo "=== Available Targets ==="
	@echo "  make all              - Build server and client"
	@echo "  make shared           - Build shared libraries for Python"
	@echo "  make run-server       - Start server on port 8080"
	@echo "  make run-server-port  - Start server on custom port"
	@echo "  make run-client       - Run C client"
	@echo "  make run-gui          - Run Python GUI client"
	@echo "  make check-server     - Check if server is running"
	@echo "  make test-connection  - Test connection to server"
	@echo "  make show-logs        - Display recent logs"
	@echo "  make network-info     - Show network configuration"
	@echo "  make setup-firewall   - Configure firewall (requires sudo)"
	@echo "  make install-deps     - Install system dependencies"
	@echo "  make clean            - Remove binaries"
	@echo "  make clean-all        - Remove binaries and data"
	@echo "  make reset            - Reset (keep users.json)"
	@echo "  make reset-hard       - Complete wipe"
	@echo "  make info             - Show this information"

# Help target (default info)
help: info

.PHONY: all clean clean-all run-server run-server-port run-client run-gui shared reset reset-hard \
        check-server test-connection show-logs install-deps setup-firewall network-info info help
