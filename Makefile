################ Target Executable and Sources ###############

TARGET     := sm64extend

SRC_FILES  := libmio0.c    \
              libsm64.c    \
              sm64extend.c \
              utils.c

OBJ_DIR     = ./obj

##################### Compiler Options #######################

WIN64_CROSS = x86_64-w64-mingw32-
WIN32_CROSS = i686-w64-mingw32-
CROSS     = $(WIN32_CROSS)
CC        = gcc
LD        = $(CC)

INCLUDES  = 
DEFS      = 
CFLAGS    = -Wall -Wextra -O2 -ffunction-sections -fdata-sections $(INCLUDES) $(DEFS) -MMD

LDFLAGS   = -s -Wl,--gc-sections
LIBS      = 

OBJ_FILES = $(addprefix $(OBJ_DIR)/,$(SRC_FILES:.c=.o))
DEP_FILES = $(OBJ_FILES:.o=.d)

WIN_OBJ_DIR = $(OBJ_DIR)_win
WIN_OBJ_FILES = $(addprefix $(WIN_OBJ_DIR)/,$(SRC_FILES:.c=.o))
WIN_DEP_FILES = $(WIN_OBJ_FILES:.o=.d)
######################## Targets #############################

default: all

all: $(TARGET)

$(OBJ_DIR)/%.o: %.c
	@[ -d $(OBJ_DIR) ] || mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -o $@ -c $<

$(WIN_OBJ_DIR)/%.o: %.c
	@[ -d $(WIN_OBJ_DIR) ] || mkdir -p $(WIN_OBJ_DIR)
	$(CROSS)$(CC) $(CFLAGS) -o $@ -c $<

$(TARGET): $(OBJ_FILES)
	$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

$(TARGET).exe: $(WIN_OBJ_FILES)
	$(CROSS)$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

mio0: libmio0.c libmio0.h
	$(CC) -DMIO0_TEST $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET) $(TARGET).exe $(OBJ_FILES) $(WIN_OBJ_FILES) $(DEP_FILES) $(WIN_DEP_FILES)
	-@[ -d $(OBJ_DIR) ] && rmdir --ignore-fail-on-non-empty $(OBJ_DIR)
	-@[ -d $(WIN_OBJ_DIR) ] && rmdir --ignore-fail-on-non-empty $(WIN_OBJ_DIR)

.PHONY: all clean default

#################### Dependency Files ########################

-include $(DEP_FILES)
