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

# Run the client (connects to localhost)
run-client: $(BIN_DIR)/client
	./$(BIN_DIR)/client

# Build both shared libs for Python integration
shared: $(BIN_DIR)/server.so $(BIN_DIR)/client.so

# ================================
# CLEANUP
# ================================
clean:
	rm -rf $(BIN_DIR)

reset:
	rm -rf $(BIN_DIR) $(DATA_DIR)/logs $(DATA_DIR)/storage

.PHONY: all clean run-server run-client shared reset
