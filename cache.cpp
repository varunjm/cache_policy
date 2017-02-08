#include <iostream>
#include <fstream>
#include <cstring>
#include <cstdlib>
#include <cmath>
#define ADDRESS_WIDTH 48

using namespace std;

enum assoc{DIRECT=1, TWO_WAY, FOUR_WAY=4, EIGHT_WAY=8};
enum rep_p{LRU, FIFO};

class cache {
    // Cache character
    int block_size;
    int size;
    int associativity;
    bool repl_policy;
    int set_count;

    // Address split
    int index_width;
    int tag_width;
    int block_offset_width;

    // Read/write statistics
    unsigned long read_miss;
    unsigned long read_hit;
    unsigned long write_miss;
    unsigned long write_hit;


    long **memory, **LRU_counter;
    long *FIFO_current;
    bool **dirty, **empty;

public:
    cache(int b, int s, int a, bool rp) {
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
        dirty = (bool **) malloc (sizeof(bool *) * set_count);
        empty = (bool **) malloc (sizeof(bool *) * set_count);
        for(int i=0; i<set_count; i++) {
            memory[i] = (long *) malloc (sizeof(long) * associativity);
            dirty[i] = (bool *) malloc (sizeof(bool) * associativity);
            empty[i] = (bool *) malloc (sizeof(bool) * associativity);
            memset(dirty[i], 0, associativity * sizeof(bool));
            memset(empty[i], 0, associativity * sizeof(bool));
        }

        if(repl_policy == FIFO) {
            FIFO_current = (long *)malloc(sizeof(long) * set_count);
            memset(FIFO_current, 0, sizeof(long) * set_count);
        } else {
            LRU_counter = (long **)malloc(sizeof(long *) * set_count);
            for(int i=0; i<set_count; i++) {
                LRU_counter[i] = (long *)malloc(sizeof(long) * set_count);
                for(int j=0; j<associativity; j++) {
                    LRU_counter[i][j] = j;
                }
            }
        }

    }

    void print_statistics() {
        cout << read_miss << endl;
        cout << read_hit << endl;
        // cout << write_miss << endl;
        // cout << write_hit << endl;

    }
    void replace(long index, long tag) {
        if(repl_policy) {       // FIFO
            memory[index][FIFO_current[index]] = tag;
            FIFO_current[index] = (FIFO_current[index] + 1) % associativity;
        } else {                // LRU

        }
    }

    void update() {}

    void read(long address) {
        long index = (address >> block_offset_width) % (1 << index_width); // set number
        long tag = (address >> (block_offset_width + index_width)); // value stored
        bool hit = false;
        int empty_index = -1;
        for(int i=0; i<associativity; i++) {
            if(memory[index][i] == tag) {
                hit = true;
                break;
            }
            if(empty_index == -1 && empty[index][i] == 0) {
                empty_index = i;
            }
        }
        if(!hit) {
            read_miss++;
            if(empty_index == -1) {
                replace(index, tag);       // need extra variable for replacement policy (queue or fifo)
            } else {
                memory[index][empty_index] = tag;
                empty[index][empty_index] = 1;                  // not empty anymore
            }
        }
        else {
            read_hit++;
            if(repl_policy == 0)        // LRU
                update();               // update least recently used
        }
    }

    void write(long address) {
        /* insert code here */
    }
};


int main(int argc, char *argv[]) {
    // <BLOCKSIZE> <L1_SIZE> <L1_ASSOC> <L2_SIZE> <L2_ASSOC> <REPL_POLICY> <INCLUSION> <TRACE_FILE>
    int block_size = 1024;
    int l2_size = 65536;
    bool repl_policy = FIFO;
    char trace_file[20] = "test1.txt";
    // int inclusion =

    char read_write, address[20];
    long val;
    ifstream fin(trace_file, ifstream::binary);
    cache L2(block_size, l2_size, TWO_WAY, repl_policy);

    while(!fin.eof()) {
        fin >> read_write >> address;
        if(fin.eof())
            break;
        val = strtol(address, NULL, 16);

        if(read_write == 'r')
            L2.read(val);
        else
            L2.write(val);
    }
    L2.print_statistics();
    return 0;
}
