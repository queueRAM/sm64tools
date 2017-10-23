TARGET = example1.bin

BIN_DIR = ./bin

AS = armips
MIO0 = ../../mio0
N64GRAPHICS = ../../n64graphics

# targets

default: $(TARGET)

example1.bin: example1.asm bin/texture_block.bin.gz bin/texture_block.mio0 bin/texture_ia16.bin bin/texture_rgba16.bin
	$(AS) $<

# graphics conversion
bin/texture_rgba16.bin: textures/heart.0.rgba16.png
	@[ -d $(BIN_DIR) ] || mkdir -p $(BIN_DIR)
	touch $@
	$(N64GRAPHICS) $@ $<

bin/texture_ia16.bin: textures/heart.0.ia16.png
	@[ -d $(BIN_DIR) ] || mkdir -p $(BIN_DIR)
	touch $@
	$(N64GRAPHICS) $@ $<

# concatenate texture files to form texture_block
bin/texture_block.bin: bin/texture_rgba16.bin bin/texture_ia16.bin
	@[ -d $(BIN_DIR) ] || mkdir -p $(BIN_DIR)
	cat $^ > $@

bin/texture_block.bin.gz: bin/texture_block.bin
	@[ -d $(BIN_DIR) ] || mkdir -p $(BIN_DIR)
	gzip -c $< > $@

bin/texture_block.mio0: bin/texture_block.bin
	@[ -d $(BIN_DIR) ] || mkdir -p $(BIN_DIR)
	$(MIO0) -c $< $@

clean:
	rm -f $(TARGET)

.PHONY: clean default
