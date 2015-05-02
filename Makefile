################ Target Executable and Sources ###############

EXTEND_TARGET := sm64extend
COMPRESS_TARGET := sm64compress
SPLIT_TARGET  := n64split
CKSUM_TARGET  := n64cksum
MIO0_TARGET   := mio0
F3D_TARGET    := f3d
N64GRAPHICS_TARGET := n64graphics
MIPSDISASM_TARGET := mipsdisasm
SM64_LIB      := libsm64.a

LIB_SRC_FILES  := libmio0.c    \
                  libsm64.c    \
                  utils.c

EXTEND_SRC_FILES := sm64extend.c

COMPRESS_SRC_FILES := sm64compress.c

F3D_SRC_FILES := f3d.c

MIPSDISASM_SRC_FILES := config.c \
                        mipsdisasm.c \
                        utils.c

CKSUM_SRC_FILES := n64cksum.c

N64GRAPHICS_SRC_FILES := n64graphics.c \
                         utils.c

SPLIT_SRC_FILES := config.c \
                   libmio0.c \
                   mipsdisasm.c \
                   n64graphics.c \
                   n64split.c \
                   utils.c

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
SPLIT_LIBS = -lcapstone -lpng -lconfig

LIB_OBJ_FILES = $(addprefix $(OBJ_DIR)/,$(LIB_SRC_FILES:.c=.o))
EXTEND_OBJ_FILES = $(addprefix $(OBJ_DIR)/,$(EXTEND_SRC_FILES:.c=.o))
COMPRESS_OBJ_FILES = $(addprefix $(OBJ_DIR)/,$(COMPRESS_SRC_FILES:.c=.o))
CKSUM_OBJ_FILES = $(addprefix $(OBJ_DIR)/,$(CKSUM_SRC_FILES:.c=.o))
SPLIT_OBJ_FILES = $(addprefix $(OBJ_DIR)/,$(SPLIT_SRC_FILES:.c=.o))
F3D_OBJ_FILES = $(addprefix $(OBJ_DIR)/,$(F3D_SRC_FILES:.c=.o))
OBJ_FILES = $(LIB_OBJ_FILES) $(EXTEND_OBJ_FILES) $(COMPRESS_OBJ_FILES) $(SPLIT_OBJ_FILES)
DEP_FILES = $(OBJ_FILES:.o=.d)

######################## Targets #############################

default: all

all: $(EXTEND_TARGET) $(COMPRESS_TARGET) $(MIO0_TARGET) $(CKSUM_TARGET) $(SPLIT_TARGET) $(F3D_TARGET) $(N64GRAPHICS_TARGET) $(MIPSDISASM_TARGET)

$(OBJ_DIR)/%.o: %.c
	@[ -d $(OBJ_DIR) ] || mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -o $@ -c $<

$(EXTEND_TARGET): $(EXTEND_OBJ_FILES) $(SM64_LIB)
	$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

$(COMPRESS_TARGET): $(COMPRESS_OBJ_FILES) $(SM64_LIB)
	$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

$(SPLIT_TARGET): $(SPLIT_OBJ_FILES)
	$(LD) $(LDFLAGS) -o $@ $^ $(SPLIT_LIBS)

$(SM64_LIB): $(LIB_OBJ_FILES)
	rm -f $@
	$(AR) rcs $@ $^

$(MIO0_TARGET): libmio0.c libmio0.h
	$(CC) $(CFLAGS) -DMIO0_TEST $(LDFLAGS) -o $@ $<

$(F3D_TARGET): $(F3D_OBJ_FILES) $(SM64_LIB)
	$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

$(MIPSDISASM_TARGET): $(MIPSDISASM_SRC_FILES)
	$(CC) $(CFLAGS) -DMIPSDISASM_STANDALONE $^ $(LDFLAGS) -o $@ -lcapstone -lconfig

$(CKSUM_TARGET): $(CKSUM_OBJ_FILES) $(SM64_LIB)
	$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

$(N64GRAPHICS_TARGET): $(N64GRAPHICS_SRC_FILES)
	$(CC) $(CFLAGS) -DN64GRAPHICS_STANDALONE $^ $(LDFLAGS) -o $@ -lpng -lz

clean:
	rm -f $(OBJ_FILES) $(DEP_FILES) $(SM64_LIB) $(MIO0_TARGET)
	rm -f $(EXTEND_TARGET) $(EXTEND_TARGET).exe
	rm -f $(COMPRESS_TARGET) $(COMPRESS_TARGET).exe
	rm -f $(CKSUM_TARGET) $(CKSUM_TARGET).exe
	rm -f $(MIO0_TARGET) $(MIO0_TARGET).exe
	rm -f $(SPLIT_TARGET) $(SPLIT_TARGET).exe
	rm -f $(F3D_TARGET) $(F3D_TARGET).exe
	-@[ -d $(OBJ_DIR) ] && rmdir --ignore-fail-on-non-empty $(OBJ_DIR)

.PHONY: all clean default

#################### Dependency Files ########################

-include $(DEP_FILES)
