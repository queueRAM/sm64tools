################ Target Executable and Sources ###############

TARGET := montage

SRC_FILES  := montage.c \
              ../config.c \
              ../utils.c

##################### Compiler Options #######################

WIN64_CROSS = x86_64-w64-mingw32-
WIN32_CROSS = i686-w64-mingw32-
#CROSS     = $(WIN32_CROSS)
CC        = $(CROSS)gcc
LD        = $(CC)

INCLUDES  = 
DEFS      = 
CFLAGS    = -Wall -Wextra -O2 -ffunction-sections -fdata-sections $(INCLUDES) $(DEFS)

LDFLAGS   = -s -Wl,--gc-sections
LIBS      = -lconfig

######################## Targets #############################

default: all

all: $(TARGET) matchsigs sm64walk

$(TARGET): $(SRC_FILES)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

matchsigs: match_signatures.c ../utils.c
	$(CC) $(CFLAGS) -o $@ $^ -lcapstone

sm64walk: sm64walk.c ../utils.c ../libsm64.a
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(TARGET)

.PHONY: all clean default

