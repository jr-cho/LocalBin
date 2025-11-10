# === Compiler Settings ===
CC      = clang
CFLAGS  = -Wall -pthread -Iinclude

# === Source and Output ===
SRC     = $(wildcard src/*.c)
OUTDIR  = bin
OUT     = $(OUTDIR)/server

# === Default Target ===
all: $(OUT)

# === Build Rule ===
$(OUT): $(SRC)
	@mkdir -p $(OUTDIR)
	@echo "[INFO] Compiling source files:"
	@echo "       $(SRC)"
	$(CC) $(CFLAGS) $(SRC) -o $(OUT)
	@echo "[SUCCESS] Build complete → $(OUT)"

# === Clean Rule ===
clean:
	rm -rf $(OUTDIR)
	@echo "[INFO] Cleaned build artifacts"

