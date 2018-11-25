nasm -f macho64 dblib.asm && \
gcc dblib.o dbtest.c -O2 -o dbtest && \
./dbtest
