ASL=/usr/local/bin/asl
P2BIN=/usr/local/bin/p2bin
P2HEX=/usr/local/bin/p2hex


diag.bin: diag.asm
	$(ASL) -cpu 8086 -L diag.asm -o diag.p
	$(P2BIN) diag.p diag.bin

upload: diag.bin
	picorom upload burroughs diag.bin 32kbit

upload-bios: ../scott-roms/h3-72-00077.BIN
	picorom upload burroughs ../scott-roms/h3-72-00077.BIN 32kbit 

clean:
	rm -f *.p *.bin

