FILES = ./build/kernel.asm.o ./build/kernel.c.o
FLAGS =  -g -ffreestanding -nostdlib -nostartfiles -Wall -O0 -Iinc

all:
	nasm -f bin ./src/boot.asm -o ./bin/boot.bin
	nasm -f elf ./src/kernel.asm -o ./build/kernel.asm.o
	i686-elf-gcc -I./src $(FLAGS) -std=gnu99 -c ./src/kernel.c -o ./build/kernel.c.o
	i686-elf-ld -g -relocatable $(FILES) -o ./build/complete_kernel.o
	i686-elf-gcc $(FLAGS) -fno-use-linker-plugin -T ./src/linkerScript.ld -o ./bin/kernel.bin -ffreestanding -O0 -nostdlib ./build/complete_kernel.o
#	i686-elf-gcc $(FLAGS) -T ./src/linkerScript.ld -o ./bin/kernel.bin -ffreestanding -O0 -nostdlib ./build/complete_kernel.o

	dd if=./bin/boot.bin >> ./bin/os.bin
	dd if=./bin/kernel.bin >> ./bin/os.bin
	dd if=/dev/zero bs=512 count=8 >> ./bin/os.bin

clean:
	rm -f ./bin/boot.bin
	rm -f ./bin/kernel.bin
	rm -f ./bin/os.bin
	rm -f ./build/kernel.asm.o
	rm -f ./build/kernel.c.o
	rm -f ./build/complete_kernel.o