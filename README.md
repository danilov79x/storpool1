cmake -S . -B build && cmake --build build -j
./build/generate_uint32 -o r1b.bin -n 1000000000 -m random
./build/unique_count r1b.bin 
