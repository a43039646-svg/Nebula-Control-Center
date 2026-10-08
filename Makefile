CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -O2 -D_GNU_SOURCE -DNEBULA_LOCALEDIR=\"/usr/local/share/nebula-control-center/locales\"
GTK_CFLAGS := $(shell pkg-config --cflags gtk4)
GTK_LIBS := $(shell pkg-config --libs gtk4)

TARGET = nebula-control-center
SOURCES = src/main.c src/system_info.c src/process_manager.c src/storage.c src/network.c src/services.c src/i18n.c src/doctor.c src/startup.c src/gaming.c src/permissions.c
OBJECTS = $(SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@ $(GTK_LIBS) -lm

src/%.o: src/%.c
	$(CC) $(CFLAGS) $(GTK_CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJECTS)

install: $(TARGET)
	install -Dm755 $(TARGET) /usr/local/bin/$(TARGET)
	install -Dm644 org.nebula.ControlCenter.desktop /usr/local/share/applications/org.nebula.ControlCenter.desktop
	install -d /usr/local/share/nebula-control-center/locales
	install -Dm644 locales/*.lang /usr/local/share/nebula-control-center/locales/

uninstall:
	rm -f /usr/local/bin/$(TARGET)
	rm -f /usr/local/share/applications/org.nebula.ControlCenter.desktop
	rm -rf /usr/local/share/nebula-control-center

.PHONY: all clean install uninstall
