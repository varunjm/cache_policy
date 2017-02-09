#include <iostream>
#include <fstream>
#include <cstring>
#include <cstdlib>
#include <cmath>
#define ADDRESS_WIDTH 48
#define DEBUG 0
using namespace std;

enum assoc{DIRECT=1, TWO_WAY, FOUR_WAY=4, EIGHT_WAY=8};
enum rep_p{LRU, FIFO};
enum rw{READ=0, WRITE=1};

class cache {
private:
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
    unsigned long read_counter;
    unsigned long write_counter;
    unsigned long read_miss;
    unsigned long read_hit;
    unsigned long write_miss;
    unsigned long write_hit;
    unsigned long write_back;

    long **memory, **LRU_counter;
    long *FIFO_current;
    bool **dirty, **empty;

    void replace(long index, long tag, int read_write) {
        long way;

        if(repl_policy) {       // FIFO
            way = FIFO_current[index];
            memory[index][way] = tag;
            FIFO_current[index] = (way + 1) % associativity;
        } else {                // LRU
            for(int j=0; j<associativity; j++) {
                if(LRU_counter[index][j] == associativity-1) {
                    LRU_counter[index][j] = 0;
                    memory[index][j] = tag;
                    way = j;
                } else {
                    LRU_counter[index][j]++;
                }
            }
        }
        if(dirty[index][way])                   // Write back if previous block was dirty
            write_back++;
        dirty[index][way] = read_write;         // Set dirty bit if write operation, else zero
    }

    void update_counter(long index, long tag, int i) {
        for(int j=0; j<associativity; j++) {
            if(LRU_counter[index][i] > LRU_counter[index][j])
                LRU_counter[index][j]++;            // Get incremented.
        }
        LRU_counter[index][i] = 0;                 // This becomes the most recently used
    }

public:
    cache(int b, int s, int a, bool rp) {
        block_size = b;
        size = s;
        associativity = a;
        repl_policy = rp;

        read_hit = write_hit = read_miss = write_miss = read_counter = write_counter = write_back = 0;
        set_count = size/(block_size*associativity);
        index_width = log2(set_count);
        block_offset_width = log2(block_size);
        tag_width = ADDRESS_WIDTH - index_width - block_offset_width;

        if(DEBUG) {
            cout << "tag_width " << tag_width << endl;
            cout << "index_width " << index_width << endl;
            cout << "block_offset_width " << block_offset_width << endl;
        }


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
        cout << "Read : " << read_counter << endl;
        cout << "Write : " << write_counter << endl;
        cout << "read_miss : " << read_miss << endl;
        cout << "read_hit : " << read_hit << endl;
        cout << "write_miss : " << write_miss << endl;
        cout << "write_hit : " << write_hit << endl;
        cout << "write_back : " << write_back << endl;
    }

    void print_lru_counters(long index) {
        for(int i=0; i<associativity; i++)
            cout << LRU_counter[index][i] << " ";
        cout << endl;
    }

    void read(long address) {
        long index = (address >> block_offset_width) % (1 << index_width); // set number
        long tag = (address >> (block_offset_width + index_width)); // value stored
        bool hit = false;
        int empty_index = -1, i;

        read_counter++;
        for(i=0; i<associativity; i++) {
            if(memory[index][i] == tag) {
                hit = true;
                break;
            }
            if(empty_index == -1 && empty[index][i] == 0) {
                empty_index = i;
            }
        }
        if(hit) {
            read_hit++;
            if(repl_policy == LRU)
                update_counter(index, tag, i);        // update least recently used
        }
        else {
            read_miss++;
            if(empty_index == -1 || repl_policy == LRU) {
                replace(index, tag, READ);       // need extra variable for replacement policy (queue or fifo)
            } else {
                memory[index][empty_index] = tag;
                empty[index][empty_index] = 1;                  // not empty anymore
            }
        }
        if(DEBUG) {
            cout << READ << " " << index <<  " " << tag << " " << hit << endl;
        }
    }

    void write(long address) {
        long index = (address >> block_offset_width) % (1 << index_width); // set number
        long tag = (address >> (block_offset_width + index_width)); // value stored
        bool hit = false;
        int empty_index = -1, i;

        write_counter++;
        for(i=0; i<associativity; i++) {
            if(memory[index][i] == tag) {
                hit = true;
                break;
            }
            if(empty_index == -1 && empty[index][i] == 0) {
                empty_index = i;
            }
        }
        if(hit) {
            write_hit++;
            dirty[index][i] = 1;
            if(repl_policy == LRU)
                update_counter(index, tag, i);        // update least recently used
        }
        else {
            write_miss++;
            if(empty_index == -1 || repl_policy == LRU) {
                replace(index, tag, WRITE);       // need extra variable for replacement policy (queue or fifo)
            } else {
                memory[index][empty_index] = tag;
                empty[index][empty_index] = 1;                  // not empty anymore
                dirty[index][empty_index] = 1;
            }
        }
        if(DEBUG) {
            cout << WRITE << " " << index <<  " " << tag << " " << hit << endl;
        }    }
};


int main(int argc, char *argv[]) {
    // <BLOCKSIZE> <L1_SIZE> <L1_ASSOC> <L2_SIZE> <L2_ASSOC> <REPL_POLICY> <INCLUSION> <TRACE_FILE>
    int block_size = 64;
    int l2_size = 262144;
    bool repl_policy = FIFO;
    // char trace_file[20] = "test1.txt";
    char trace_file[20] = "./Traces/GCC.t";
    // int inclusion =

    char read_write, address[20];
    long val;
    ifstream fin(trace_file, ifstream::binary);
    cache L2(block_size, l2_size, EIGHT_WAY, repl_policy);

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
