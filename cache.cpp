#include <iostream>
#include <cstdint>
#include <fstream>
#include <string.h>

using namespace std;

int main(int argc, char *argv[]) {
    // <BLOCKSIZE> <L1_SIZE> <L1_ASSOC> <L2_SIZE> <L2_ASSOC> <REPL_POLICY> <INCLUSION> <TRACE_FILE>
    uint64_t block_size = 1024;
    uint64_t l1_size = 65536;
    bool repl_policy = 1;   // FIFO
    char trace_file[20] = "test.txt";
    char read_write, address[20];
    uint64_t val;
    // uint64_t l2_size =
    // int inclusion =
    ifstream fin(trace_file, ifstream::binary);

    while(!fin.eof()) {
        fin >> read_write >> address;
        val = (uint64_t) strtol(word1, NULL, 16);
        return 0;
    }
}
