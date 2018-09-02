TARGET = example1.bin

BIN_DIR = ./bin

AS = armips
MIO0 = ../../mio0
N64GRAPHICS = ../../n64graphics

# targets

default: $(TARGET)

example1.bin: example1.asm bin/texture_block.bin.gz bin/texture_block.mio0 bin/texture_ia16.bin bin/texture_rgba16.bin
	$(AS) $<

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# graphics conversion
$(BIN_DIR)/texture_rgba16.bin: textures/heart.0.rgba16.png $(BIN_DIR)
	$(N64GRAPHICS) -f rgba16 -g $< -i $@

$(BIN_DIR)/texture_ia16.bin: textures/heart.0.ia16.png $(BIN_DIR)
	$(N64GRAPHICS) -f ia16 -g $< -i $@

# concatenate texture files to form texture_block
$(BIN_DIR)/texture_block.bin: bin/texture_rgba16.bin bin/texture_ia16.bin
	cat $^ > $@

$(BIN_DIR)/texture_block.bin.gz: bin/texture_block.bin
	@[ -d $(BIN_DIR) ] || mkdir -p $(BIN_DIR)
	gzip -c $< > $@

$(BIN_DIR)/texture_block.mio0: bin/texture_block.bin
	@[ -d $(BIN_DIR) ] || mkdir -p $(BIN_DIR)
	$(MIO0) -c $< $@

clean:
	rm -rf $(TARGET) $(BIN_DIR)

.PHONY: clean default
