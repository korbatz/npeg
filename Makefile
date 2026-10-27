CC ?= cc
CFLAGS ?= -O2 -std=gnu11 -Wall -Wextra -Wpedantic

# Requires VLC 3.x development headers. The plugin ABI matches your system's
# VLC installation; do not copy Debian-built .so files onto Fedora.
VLC_CFLAGS ?= $(shell pkg-config --cflags vlc-plugin 2>/dev/null)
VLC_LDFLAGS ?= $(shell pkg-config --libs vlc-plugin 2>/dev/null)
# Use VLC 3.x plugin SDK from the same installation as the VLC executable.

.PHONY: all clean test test-mock install-user check-vlc-sdk
all: check-vlc-sdk libnpeg_plugin.so npeg-convert

check-vlc-sdk:
	@pkg-config --exists vlc-plugin || { echo "VLC development SDK missing: install vlc-devel (Fedora)" >&2; exit 1; }

libnpeg_plugin.so: src/vlc_npeg.c src/npeg.c src/npeg.h
	$(CC) $(CFLAGS) -D_FILE_OFFSET_BITS=64 -DMODULE_STRING=\"npeg\" -D__PLUGIN__ -fPIC -shared \
	  $(VLC_CFLAGS) src/vlc_npeg.c src/npeg.c -o $@ $(VLC_LDFLAGS) -lm

npeg-convert: src/npeg-convert.c src/npeg.c src/npeg.h
	$(CC) $(CFLAGS) -D_POSIX_C_SOURCE=200809L src/npeg-convert.c src/npeg.c -o $@ -lm

install-user: libnpeg_plugin.so
	@bash scripts/install-user.sh

test: npeg-convert test-mock
	@bash tests/smoke.sh samples/video.npeg samples/prep2.npeg

test-mock:
	$(CC) -O2 -std=gnu11 -Wall -Wextra -Wpedantic -Wno-unused-function -Itests/fake-vlc tests/plugin_integration.c src/npeg.c -lm -o tests/plugin-integration
	./tests/plugin-integration samples/video.npeg samples/prep2.npeg

clean:
	rm -f libnpeg_plugin.so npeg-convert tests/plugin-integration
