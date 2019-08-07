################ Target Executable and Sources ###############

SM64_LIB        := libsm64.a
COMPRESS_TARGET := sm64compress
CKSUM_TARGET    := n64cksum
DISASM_TARGET   := mipsdisasm
EXTEND_TARGET   := sm64extend
F3D_TARGET      := f3d
F3D2OBJ_TARGET  := f3d2obj
GEO_TARGET      := sm64geo
GRAPHICS_TARGET := n64graphics
MIO0_TARGET     := mio0
SPLIT_TARGET    := n64split
WALK_TARGET     := sm64walk

LIB_SRC_FILES  := libmio0.c    \
                  libsm64.c    \
                  libsfx.c     \
                  utils.c

CKSUM_SRC_FILES := n64cksum.c

COMPRESS_SRC_FILES := sm64compress.c

DISASM_SRC_FILES := mipsdisasm.c \
                    utils.c

EXTEND_SRC_FILES := sm64extend.c

F3D_SRC_FILES := f3d.c \
                 utils.c

F3D2OBJ_SRC_FILES := blast.c \
                     f3d2obj.c \
                     n64graphics.c \
                     utils.c

GEO_SRC_FILES := sm64geo.c \
                 utils.c

GRAPHICS_SRC_FILES := n64graphics.c \
                      utils.c

MI0_SRC_FILES := libmio0.c \
                 libmio0.h

SPLIT_SRC_FILES := blast.c \
                   libmio0.c \
                   libsfx.c \
                   mipsdisasm.c \
                   n64graphics.c \
                   n64split/n64split.c \
                   n64split/n64split.sm64.geo.c \
                   n64split/n64split.sm64.behavior.c \
                   n64split/n64split.sm64.collision.c \
                   n64split/n64split.sound.c \
                   strutils.c \
                   utils.c \
                   yamlconfig.c

WALK_SRC_FILES := sm64walk.c

OBJ_DIR     = ./obj
BIN_DIR     = ./bin
SPLIT_DIR   = $(OBJ_DIR)/n64split

##################### Compiler Options #######################

WIN64_CROSS = x86_64-w64-mingw32-
WIN32_CROSS = i686-w64-mingw32-
#CROSS     = $(WIN32_CROSS)
CC        = $(CROSS)gcc
LD        = $(CC)
AR        = $(CROSS)ar

INCLUDES  = -I. -I./ext
DEFS      = 
# Release flags
CFLAGS    = -Wall -Wextra -Wno-format-overflow -O2 -ffunction-sections -fdata-sections $(INCLUDES) $(DEFS) -MMD
LDFLAGS   = -s -Wl,--gc-sections
# Debug flags
#CFLAGS    = -Wall -Wextra -O0 -g $(INCLUDES) $(DEFS) -MMD
#LDFLAGS   =
LIBS      = 
SPLIT_LIBS = -lcapstone -lyaml -lz

LIB_OBJ_FILES = $(addprefix $(OBJ_DIR)/,$(LIB_SRC_FILES:.c=.o))
CKSUM_OBJ_FILES = $(addprefix $(OBJ_DIR)/,$(CKSUM_SRC_FILES:.c=.o))
COMPRESS_OBJ_FILES = $(addprefix $(OBJ_DIR)/,$(COMPRESS_SRC_FILES:.c=.o))
EXTEND_OBJ_FILES = $(addprefix $(OBJ_DIR)/,$(EXTEND_SRC_FILES:.c=.o))
F3D_OBJ_FILES = $(addprefix $(OBJ_DIR)/,$(F3D_SRC_FILES:.c=.o))
F3D2OBJ_OBJ_FILES = $(addprefix $(OBJ_DIR)/,$(F3D2OBJ_SRC_FILES:.c=.o))
GEO_OBJ_FILES = $(addprefix $(OBJ_DIR)/,$(GEO_SRC_FILES:.c=.o))
MI0_OBJ_FILES = $(addprefix $(OBJ_DIR)/,$(MI0_SRC_FILES:.c=.o))
SPLIT_OBJ_FILES = $(addprefix $(OBJ_DIR)/,$(SPLIT_SRC_FILES:.c=.o))
WALK_OBJ_FILES = $(addprefix $(OBJ_DIR)/,$(WALK_SRC_FILES:.c=.o))
OBJ_FILES = $(LIB_OBJ_FILES) $(CKSUM_OBJ_FILES) $(COMPRESS_OBJ_FILES) \
            $(EXTEND_OBJ_FILES) $(F3D_OBJ_FILES) $(F3D2OBJ_OBJ_FILES) \
            $(GEO_OBJ_FILES) $(MI0_OBJ_FILES) $(SPLIT_OBJ_FILES) \
            $(WALK_OBJ_FILES)
DEP_FILES = $(OBJ_FILES:.o=.d)


######################## Targets #############################

default: all

all: $(EXTEND_TARGET) $(COMPRESS_TARGET) $(MIO0_TARGET) $(CKSUM_TARGET) \
     $(SPLIT_TARGET) $(F3D_TARGET) $(F3D2OBJ_TARGET) $(GRAPHICS_TARGET) \
     $(DISASM_TARGET) $(GEO_TARGET) $(WALK_TARGET)

$(OBJ_DIR)/%.o: %.c
	@[ -d $(OBJ_DIR) ] || mkdir -p $(OBJ_DIR)
	@[ -d $(BIN_DIR) ] || mkdir -p $(BIN_DIR)
	@[ -d $(SPLIT_DIR) ] || mkdir -p $(SPLIT_DIR)
	$(CC) $(CFLAGS) -o $@ -c $<

$(SM64_LIB): $(LIB_OBJ_FILES)
	rm -f $@
	$(AR) rcs $@ $^

$(CKSUM_TARGET): $(CKSUM_OBJ_FILES) $(SM64_LIB)
	$(LD) $(LDFLAGS) -o $(BIN_DIR)/$@ $^ $(LIBS)

$(COMPRESS_TARGET): $(COMPRESS_OBJ_FILES) $(SM64_LIB)
	$(LD) $(LDFLAGS) -o $(BIN_DIR)/$@ $^ $(LIBS)

$(EXTEND_TARGET): $(EXTEND_OBJ_FILES) $(SM64_LIB)
	$(LD) $(LDFLAGS) -o $(BIN_DIR)/$@ $^ $(LIBS)

$(F3D_TARGET): $(F3D_OBJ_FILES)
	$(LD) $(LDFLAGS) -o $(BIN_DIR)/$@ $^

$(F3D2OBJ_TARGET): $(F3D2OBJ_OBJ_FILES)
	$(LD) $(LDFLAGS) -o $(BIN_DIR)/$@ $^

$(GEO_TARGET): $(GEO_OBJ_FILES)
	$(LD) $(LDFLAGS) -o $(BIN_DIR)/$@ $^

$(GRAPHICS_TARGET): $(GRAPHICS_SRC_FILES)
	$(CC) $(CFLAGS) -DN64GRAPHICS_STANDALONE $^ $(LDFLAGS) -o $(BIN_DIR)/$@

$(MIO0_TARGET): $(MI0_SRC_FILES)
	$(CC) $(CFLAGS) -DMIO0_STANDALONE $(LDFLAGS) -o $(BIN_DIR)/$@ $<

$(DISASM_TARGET): $(DISASM_SRC_FILES)
	$(CC) $(CFLAGS) -DMIPSDISASM_STANDALONE $^ $(LDFLAGS) -o $(BIN_DIR)/$@ -lcapstone

$(SPLIT_TARGET): $(SPLIT_OBJ_FILES)
	$(LD) $(LDFLAGS) -o $(BIN_DIR)/$@ $^ $(SPLIT_LIBS)

$(WALK_TARGET): $(WALK_SRC_FILES) $(SM64_LIB)
	$(CC) $(CFLAGS) -o $(BIN_DIR)/$@ $^

rawmips: rawmips.c utils.c
	$(CC) $(CFLAGS) -o $(BIN_DIR)/$@ $^ -lcapstone

clean:
	rm -f $(OBJ_FILES) $(DEP_FILES) $(SM64_LIB) $(MIO0_TARGET) $(BIN_DIR)/*.d
	rm -f $(BIN_DIR)/$(CKSUM_TARGET) $(BIN_DIR)/$(CKSUM_TARGET).exe
	rm -f $(BIN_DIR)/$(COMPRESS_TARGET) $(BIN_DIR)/$(COMPRESS_TARGET).exe
	rm -f $(BIN_DIR)/$(DISASM_TARGET) $(BIN_DIR)/$(DISASM_TARGET).exe
	rm -f $(BIN_DIR)/$(EXTEND_TARGET) $(BIN_DIR)/$(EXTEND_TARGET).exe
	rm -f $(BIN_DIR)/$(F3D_TARGET) $(BIN_DIR)/$(F3D_TARGET).exe
	rm -f $(BIN_DIR)/$(F3D2OBJ_TARGET) $(BIN_DIR)/$(F3D2OBJ_TARGET).exe
	rm -f $(BIN_DIR)/$(GEO_TARGET) $(BIN_DIR)/$(GEO_TARGET).exe
	rm -f $(BIN_DIR)/$(MIO0_TARGET) $(BIN_DIR)/$(MIO0_TARGET).exe
	rm -f $(BIN_DIR)/$(GRAPHICS_TARGET) $(BIN_DIR)/$(GRAPHICS_TARGET).exe
	rm -f $(BIN_DIR)/$(SPLIT_TARGET) $(BIN_DIR)/$(SPLIT_TARGET).exe
	rm -f $(BIN_DIR)/$(WALK_TARGET) $(BIN_DIR)/$(WALK_TARGET).exe
	-@[ -d $(SPLIT_DIR) ] && rmdir --ignore-fail-on-non-empty $(SPLIT_DIR)
	-@[ -d $(OBJ_DIR) ] && rmdir --ignore-fail-on-non-empty $(OBJ_DIR)
	-@[ -d $(BIN_DIR) ] && rmdir --ignore-fail-on-non-empty $(BIN_DIR)

.PHONY: all clean default

#################### Dependency Files ########################

-include $(DEP_FILES)
