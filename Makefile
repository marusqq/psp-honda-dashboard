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
	src/telemetry/derived.o \
	src/telemetry/filter.o \
	src/telemetry/units.o \
	src/ui/renderer.o \
	src/ui/gauge.o \
	src/ui/dashboard.o \
	src/ui/themes.o \
	src/ui/setup.o \
	src/input/controls.o \
	src/config/settings.o \
	src/utils/log.o \
	src/utils/time.o \
	src/utils/memory.o \
	src/utils/font.o \
	src/utils/stats_log.o

# INCDIR is expanded by build.mak with addprefix -I, so do NOT use -I$(INCDIR) here
INCDIR   = include
CFLAGS   = -O2 -G0 -Wall -Wextra
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS  = $(CFLAGS)

BUILD_PRX       = 1
PSP_FW_VERSION  = 371

LIBDIR  =
LDFLAGS =
# Don't duplicate -lpspnet/-lpspnet_apctl; build.mak appends them
LIBS    = -lpspgu -lpspgum -lpsprtc -lpspnet_inet -lpspwlan -lpsputility -lm

EXTRA_TARGETS   = EBOOT.PBP
PSP_EBOOT_TITLE = PSP OBD2 Dashboard
PSP_EBOOT_ICON  = assets/icon0.png

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
