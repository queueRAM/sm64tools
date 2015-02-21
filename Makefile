################ Target Executable and Sources ###############

TARGET     := sm64extend

SRC_FILES  := libmio0.c    \
              main.c       \
              sm64_tools.c \
              utils.c

OBJ_DIR     = ./obj

##################### Compiler Options #######################

CC        = gcc
LD        = $(CC)

INCLUDES  = 
DEFS      = 
CFLAGS    = -Wall -Wextra -O2 $(INCLUDES) $(DEFS) -MMD

LDFLAGS   = 
LIBS      = 

OBJ_FILES = $(addprefix $(OBJ_DIR)/,$(SRC_FILES:.c=.o))
DEP_FILES = $(OBJ_FILES:.o=.d)

######################## Targets #############################

default: all

all: $(TARGET)

$(OBJ_DIR)/%.o: %.c
	@[ -d $(OBJ_DIR) ] || mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -o $@ -c $<

$(TARGET): $(OBJ_FILES)
	$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

clean:
	rm -f $(TARGET) $(OBJ_FILES) $(DEP_FILES)
	@rmdir --ignore-fail-on-non-empty $(OBJ_DIR)

.PHONY: all clean default

#################### Dependency Files ########################

-include $(DEP_FILES)
