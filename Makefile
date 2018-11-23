FORCE:

build: FORCE
	[ -e build/ ] || mkdir build
	gcc $$(find src -name '*\.c') $$(find lib/*/src -name *\.c) -Iinclude/ -Ilib/string/ $(C_FLAGS) -o build/lc

run_test:
	make compile OPT_LEVEL=0 C_FLAGS=-g
	for f in $$(ls test/ | grep '\.l'); do \
		echo === Testing on $$f ===; \
		./build/lc test/$$f; \
	done
