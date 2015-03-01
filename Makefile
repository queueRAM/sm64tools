################ Target Executable and Sources ###############

EXTEND_TARGET := sm64extend
SHRINK_TARGET := sm64shrink
SM64_LIB      := libsm64.a

LIB_SRC_FILES  := libmio0.c    \
                  libsm64.c    \
                  utils.c

EXTEND_SRC_FILES := sm64extend.c

SHRINK_SRC_FILES := sm64shrink.c

OBJ_DIR     = ./obj

##################### Compiler Options #######################

WIN64_CROSS = x86_64-w64-mingw32-
WIN32_CROSS = i686-w64-mingw32-
#CROSS     = $(WIN32_CROSS)
CC        = $(CROSS)gcc
LD        = $(CC)
AR        = $(CROSS)ar

INCLUDES  = 
DEFS      = 
CFLAGS    = -Wall -Wextra -O2 -ffunction-sections -fdata-sections $(INCLUDES) $(DEFS) -MMD

LDFLAGS   = -s -Wl,--gc-sections
LIBS      = 

LIB_OBJ_FILES = $(addprefix $(OBJ_DIR)/,$(LIB_SRC_FILES:.c=.o))
EXTEND_OBJ_FILES = $(addprefix $(OBJ_DIR)/,$(EXTEND_SRC_FILES:.c=.o))
SHRINK_OBJ_FILES = $(addprefix $(OBJ_DIR)/,$(SHRINK_SRC_FILES:.c=.o))
OBJ_FILES = $(LIB_OBJ_FILES) $(EXTEND_OBJ_FILES) $(SHRINK_OBJ_FILES)
DEP_FILES = $(OBJ_FILES:.o=.d)

######################## Targets #############################

default: all

all: $(EXTEND_TARGET) $(SHRINK_TARGET)

$(OBJ_DIR)/%.o: %.c
	@[ -d $(OBJ_DIR) ] || mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -o $@ -c $<

$(EXTEND_TARGET): $(EXTEND_OBJ_FILES) $(SM64_LIB)
	$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

$(SHRINK_TARGET): $(SHRINK_OBJ_FILES) $(SM64_LIB)
	$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

$(SM64_LIB): $(LIB_OBJ_FILES)
	rm -f $@
	$(AR) rcs $@ $^

mio0tool: libmio0.c libmio0.h
	$(CC) -DMIO0_TEST $(CFLAGS) -o $@ $<

clean:
	rm -f $(OBJ_FILES) $(DEP_FILES) $(SM64_LIB) mio0tool
	rm -f $(EXTEND_TARGET) $(EXTEND_TARGET).exe
	rm -f $(SHRINK_TARGET) $(SHRINK_TARGET).exe
	-@[ -d $(OBJ_DIR) ] && rmdir --ignore-fail-on-non-empty $(OBJ_DIR)

.PHONY: all clean default

#################### Dependency Files ########################

-include $(DEP_FILES)
