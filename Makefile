CC ?= cc
PKG_CONFIG ?= pkg-config
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
DATADIR ?= $(PREFIX)/share
LOCALEDIR ?= $(DATADIR)/nebula-control-center/locales
# Keep the privileged helper at a stable, root-owned system path.
LIBEXECDIR ?= /usr/lib/nebula-control-center
POLKIT_ACTION_DIR ?= /usr/share/polkit-1/actions
POLKIT_POLICY = build/org.nebula.ControlCenter.policy
TARGET = nebula-control-center
HELPER = nebula-process-terminator
SOURCES = src/main.c src/system_info.c src/process_manager.c src/storage.c src/network.c src/services.c src/i18n.c src/doctor.c src/startup.c src/gaming.c src/permissions.c
OBJECTS = $(SOURCES:.c=.o)

CFLAGS ?= -std=c11 -Wall -Wextra -O2
CPPFLAGS += -D_GNU_SOURCE -DNEBULA_LOCALEDIR=\"$(LOCALEDIR)\" -DNEBULA_HELPER_PATH=\"$(LIBEXECDIR)/$(HELPER)\"
GTK_CFLAGS := $(shell $(PKG_CONFIG) --cflags gtk4 2>/dev/null)
GTK_LIBS := $(shell $(PKG_CONFIG) --libs gtk4 2>/dev/null)

.PHONY: all check-deps clean install uninstall test
all: check-deps $(TARGET) $(HELPER)

check-deps:
	@command -v $(PKG_CONFIG) >/dev/null 2>&1 || { echo "Error: pkg-config is required." >&2; exit 1; }
	@$(PKG_CONFIG) --exists gtk4 || { echo "Error: GTK4 development files are missing. Run ./install-deps.sh for your distro." >&2; exit 1; }

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJECTS) -o $@ $(GTK_LIBS) -lm

$(HELPER): src/privileged_terminator.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $< -o $@

src/%.o: src/%.c | check-deps
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GTK_CFLAGS) -c $< -o $@

$(POLKIT_POLICY): data/org.nebula.ControlCenter.policy.in
	mkdir -p build
	sed 's|@HELPER_PATH@|$(LIBEXECDIR)/$(HELPER)|g' $< > $@

clean:
	rm -f $(TARGET) $(HELPER) $(OBJECTS)
	rm -rf build

test:
	sh tests/test-privileged-terminator.sh

install: all $(POLKIT_POLICY)
	install -Dm755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)
	install -Dm755 $(HELPER) $(DESTDIR)$(LIBEXECDIR)/$(HELPER)
	install -Dm644 org.nebula.ControlCenter.desktop $(DESTDIR)$(DATADIR)/applications/org.nebula.ControlCenter.desktop
	install -Dm644 nebula-control-center.png $(DESTDIR)$(DATADIR)/icons/hicolor/512x512/apps/nebula-control-center.png
	install -d $(DESTDIR)$(LOCALEDIR)
	install -m644 locales/*.lang $(DESTDIR)$(LOCALEDIR)/
	install -Dm644 $(POLKIT_POLICY) $(DESTDIR)$(POLKIT_ACTION_DIR)/org.nebula.ControlCenter.policy
	@echo "Installed Nebula Control Center to $(DESTDIR)$(BINDIR)"
	@echo "Administrator process termination requires pkexec and an active polkit authentication agent."

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)
	rm -f $(DESTDIR)$(LIBEXECDIR)/$(HELPER)
	rmdir $(DESTDIR)$(LIBEXECDIR) 2>/dev/null || true
	rm -f $(DESTDIR)$(DATADIR)/applications/org.nebula.ControlCenter.desktop
	rm -f $(DESTDIR)$(DATADIR)/icons/hicolor/512x512/apps/nebula-control-center.png
	rm -rf $(DESTDIR)$(LOCALEDIR)
	rm -f $(DESTDIR)$(POLKIT_ACTION_DIR)/org.nebula.ControlCenter.policy
