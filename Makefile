TARGET = psp-obd2-dashboard

OBJS = \
	src/main.o \
	src/app.o \
	src/net/wifi.o \
	src/net/socket.o \
	src/net/reconnect.o \
	src/obd/elm327.o \
	src/obd/pid.o \
	src/obd/parser.o \
	src/obd/diagnostics.o \
	src/telemetry/model.o \
	src/telemetry/filter.o \
	src/telemetry/units.o \
	src/ui/renderer.o \
	src/ui/gauge.o \
	src/ui/dashboard.o \
	src/ui/themes.o \
	src/input/controls.o \
	src/config/settings.o \
	src/utils/log.o \
	src/utils/time.o \
	src/utils/memory.o \
	src/utils/font.o

INCDIR   = include
CFLAGS   = -O2 -G0 -Wall -Wextra -I$(INCDIR)
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS  = $(CFLAGS)

BUILD_PRX       = 1
PSP_FW_VERSION  = 371

LIBDIR  =
LDFLAGS =
LIBS    = -lpspnet -lpspnet_inet -lpspnet_apctl -lm

EXTRA_TARGETS   = EBOOT.PBP
PSP_EBOOT_TITLE = PSP OBD2 Dashboard
PSP_EBOOT_ICON  = assets/ICON0.PNG

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
