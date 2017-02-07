#include <iostream>
#include <cstdint>
#include <fstream>
#include <string.h>
#include <cmath>
#define ADDRESS_WIDTH 48
using namespace std;

enum assoc{DIRECT=1, TWO_WAY, FOUR_WAY=4, EIGHT_WAY=8};
class cache {
    int block_size;
    int size;
    int associativity;
    int repl_policy;
    int set_count;
    int index_width;
    int tag_width;
    int block_offset_width;
    int read_miss;
    int read_hit;
    int write_miss;
    int write_hit;
    long **memory;

    cache(int b, int s, int a, int rp) {
        block_size = b;
        size = s;
        associativity = a;
        repl_policy = rp;
        read_hit = write_hit = read_miss = write_miss = 0;
        set_count = size/(block_size*associativity);
        index_width = log2(set_count);
        block_offset_width = log2(block_size);
        tag_width = ADDRESS_WIDTH - index_width - block_offset_width;

        memory = (long **) malloc (sizeof(long *) * set_count);
        for(int i=0; i<set_count; i++) {
            memory[i] = (long *) malloc (sizeof(long) * associativity);
            memset(memory[i], -1, associativity * sizeof(long));
        }

    }
    void read(long address) {
        long index = (address >> block_offset_width) % (1 << index_width); // set number
        long tag = (address >> (block_offset_width + index_width)); // value stored]
        bool hit = false;
        int empty = -1;
        for(int i=0; i<associativity; i++) {
            if(memory[index][i] == tag) {
                hit = true;
                break;
            }
            if(memory[index][i] == -1 && empty == -1) {
                empty = i;
            }
        }
        if(!hit) {
            read_miss++;
            if(empty == -1) {
                replace(memory[index], tag, repl_policy);       // need extra variable for replacement policy (queue or fifo)
            } else {
                memory[index][empty] = tag;
            }
        }
        else {
            read_hit++;
            if(repl_policy == 0)        // LRU
                update();               // update least recently used
        }
    }

    void write(long address) {

    }

    void replace() {

    }

};


int main(int argc, char *argv[]) {
    // <BLOCKSIZE> <L1_SIZE> <L1_ASSOC> <L2_SIZE> <L2_ASSOC> <REPL_POLICY> <INCLUSION> <TRACE_FILE>
    int block_size = 1024;
    int l2_size = 65536;
    bool repl_policy = 1;   // FIFO
    char trace_file[20] = "test.txt";
    // uint64_t l2_size =
    // int inclusion =

    char read_write, address[20];
    long val;
    ifstream fin(trace_file, ifstream::binary);
    cache L2;

    L2.cache(block_size, l2_size, DIRECT, repl_policy);
    while(!fin.eof()) {
        fin >> read_write >> address;
        val = strtol(address, NULL, 16);

        if(read_write == 'r')
            L2.read(address);
        else
            L2.write(address);

        return 0;
    }
}
