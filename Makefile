.FORCE: # Pffff

build: src/* .FORCE
	[ -e build/ ] || mkdir build
	gcc $$(find src -name '*\.c') $$(find lib/*/src -name *\.c) -Iinclude/ -Ilib/string/ $(C_FLAGS) -o build/lc

run_test:
	make build OPT_LEVEL=0 C_FLAGS=-g
	./build/lc $(FILE)
	./build/lc $(FILE) > /tmp/test.asm 2>/dev/null
	nasm -f $(ARCH) -o /tmp/test.o /tmp/test.asm
	nasm -f $(ARCH) -o /tmp/dblib.o test/dblib.asm
	ld /tmp/test.o /tmp/dblib.o -o /tmp/test
	/tmp/test
