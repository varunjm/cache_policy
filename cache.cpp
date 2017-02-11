#include <iostream>
#include <fstream>
#include <cstring>
#include <cstdlib>
#include <cmath>
#define ADDRESS_WIDTH 48
#define DEBUG 1
using namespace std;

enum assoc{DIRECT=1, TWO_WAY, FOUR_WAY=4, EIGHT_WAY=8};
enum rep_p{LRU, FIFO};
enum rw{READ=0, WRITE=1};
enum incls{INCLUSIVE, NINCLUSIVE, EXCLUSIVE};


typedef struct f{
    long evicted_address;
    bool evicted;
    bool miss;
} FLAGS;

FLAGS * get_flags_node() {
    FLAGS *temp = (FLAGS *)malloc(sizeof(FLAGS));
    temp->evicted = temp->miss = false;
    temp->evicted_address = 0;

    return temp;
}

class cache {
    private:

    char name[10];
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

    long get_address(long tag, long index) {
        return (((tag << index_width) + index) << block_offset_width);
    }

    void pi(long index) {
        for(int i=0; i<associativity; i++)
            cout << memory[index][i] << " ";
        cout << endl;
        return;
    }

    void replace(long index, long tag, int read_write, FLAGS *temp) {        // Returns address of evicted block
        long way, evicted;

        if(repl_policy == FIFO) {       // FIFO
            way = FIFO_current[index];
            temp->evicted_address = get_address(memory[index][way], index);
            memory[index][way] = tag;
            empty[index][way] = 1;
            FIFO_current[index] = (way + 1) % associativity;
        } else {                // LRU
            for(int j=0; j<associativity; j++) {
                if(LRU_counter[index][j] == associativity-1) {
                    way = j;
                    temp->evicted_address = get_address(memory[index][way], index);
                    memory[index][way] = tag;
                    empty[index][way] = 1;
                    LRU_counter[index][way] = 0;
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
    cache(int b, int s, int a, bool rp, char n[]) {
        block_size = b;
        size = s;
        associativity = a;
        repl_policy = rp;
        strcpy(name, n);

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

    FLAGS * read(long address) {           // returns address of evicted block, else returns -1
        long index = (address >> block_offset_width) % (1 << index_width); // set number
        long tag = (address >> (block_offset_width + index_width)); // value stored
        bool hit = false;
        int empty_index = -1, i;
        FLAGS *temp = get_flags_node();

        read_counter++;
        for(i=0; i<associativity; i++) {
            if(memory[index][i] == tag && empty[index][i] != 0) {
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
            temp->miss = true;
            if(empty_index == -1 || repl_policy == LRU) {
                replace(index, tag, READ, temp);       // need extra variable for replacement policy (queue or fifo)
                temp->evicted = true;
            } else {
                memory[index][empty_index] = tag;
                empty[index][empty_index] = 1;                  // not empty anymore
            }
        }
        if(DEBUG) {
            cout << name << " " << READ << " " << index <<  " " << tag << " " << hit << endl;
        }
        return temp;
    }

    FLAGS * write(long address) {          // returns address of evicted block, else returns -1
        long index = (address >> block_offset_width) % (1 << index_width); // set number
        long tag = (address >> (block_offset_width + index_width)); // value stored
        bool hit = false;
        int empty_index = -1, i;
        FLAGS * temp = get_flags_node();

        write_counter++;
        for(i=0; i<associativity; i++) {
            if(memory[index][i] == tag) {
                hit = true;
                temp->miss = false;
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
            temp->miss = true;
            if(empty_index == -1 || repl_policy == LRU) {
                replace(index, tag, WRITE, temp);       // need extra variable for replacement policy (queue or fifo)
                temp->evicted = true;
            } else {
                memory[index][empty_index] = tag;
                empty[index][empty_index] = 1;                  // not empty anymore
                dirty[index][empty_index] = 1;
            }
        }
        if(DEBUG) {
            cout << name << " " << WRITE << " " << index <<  " " << tag << " " << hit << endl;
        }
        return temp;
    }

    void evict(long address) {
        long index = (address >> block_offset_width) % (1 << index_width); // set number
        long tag = (address >> (block_offset_width + index_width)); // value stored
        int i;

        for(i=0; i < associativity; i++)     // Find index of block to be evicted
            if(memory[index][i] == tag)
                break;
        if(i == associativity)
            return;

        if(repl_policy == FIFO) {
            if(FIFO_current[index] == i) {
                memory[index][i] = empty[index][i] = dirty[index][i] = 0;
                FIFO_current[index] = ( FIFO_current[index] + 1 ) % associativity;
            } else if(FIFO_current[index] > i) {
                for(++i; i<FIFO_current[index]; i++) {
                    memory[index][i-1] = memory[index][i];
                    empty[index][i-1] = empty[index][i];
                    dirty[index][i-1] = dirty[index][i];
                }
                memory[index][i-1] = empty[index][i-1] = dirty[index][i-1] = 0;   // i == FIFO_current[index]
            } else {
                for(; i>FIFO_current[index]; i--) {
                    memory[index][i] = memory[index][i-1];
                    empty[index][i] = empty[index][i-1];
                    dirty[index][i] = dirty[index][i-1];
                }
                memory[index][i] = empty[index][i] = dirty[index][i] = 0;       // i == FIFO_current[index]
                FIFO_current[index]++;
            }
            return;
        } else {                    // LRU evict policy
            int counter = LRU_counter[index][i];
            memory[index][i] = empty[index][i] = dirty[index][i] = 0;
            LRU_counter[index][i] = associativity - 1;
            for(int j=0; j<associativity; j++) {
                if(j != i && LRU_counter[index][j] > counter)
                    LRU_counter[index][j]--;
            }
        }
    }
};

class hierarchy {
    cache L1;
    cache L2;

    int inclusion;

    public:
    hierarchy(int b, int l1_size, int l1_assoc, int l2_size, int l2_assoc, bool rp, int incl, char n1[], char n2[])
        : L1(b, l1_size, l1_assoc, rp, n1), L2(b, l2_size, l2_assoc, rp, n2) {
        inclusion = incl;
    }

    void print_statistics() {
        cout << "L1 : \n";
        L1.print_statistics();
        cout << "\nL2 : \n";
        L2.print_statistics();
    }

    void read(long address) {
        FLAGS * temp;

        switch(inclusion) {
            case INCLUSIVE: {
                temp = L1.read(address);

                if(temp->miss) {
                    free(temp);
                    temp = L2.read(address);
                    if(temp->evicted) {
                        L1.evict(temp->evicted_address);
                    }
                    free(temp);
                }
            }
            case NINCLUSIVE: {

            }
            case EXCLUSIVE: {

            }
        }
    }

    void write(long address) {
        FLAGS * temp;

        switch(inclusion) {
            case INCLUSIVE: {
                temp = L1.write(address);

                if(temp->miss) {
                    free(temp);
                    temp = L2.write(address);
                    if(temp->evicted) {
                        L1.evict(temp->evicted_address);
                    }
                    free(temp);
                }
            }
            case NINCLUSIVE: {

            }
            case EXCLUSIVE: {

            }
        }
    }
};

int main(int argc, char *argv[]) {
    // <BLOCKSIZE> <L1_SIZE> <L1_ASSOC> <L2_SIZE> <L2_ASSOC> <REPL_POLICY> <INCLUSION> <TRACE_FILE>
    int block_size = 64;
    int l1_size = 32768;
    int l2_size = 262144;
    int l1_assoc = TWO_WAY;
    int l2_assoc = FOUR_WAY;
    bool repl_policy = LRU;
    // char trace_file[20] = "./Traces/GCC.t";
    char trace_file[20] = "test1.txt";
    // int inclusion =

    char read_write, address[20];
    long val;
    char n1[20] = "L1", n2[20] = "L2";
    ifstream fin(trace_file, ifstream::binary);
    hierarchy H(block_size, l1_size, l1_assoc, l2_size, l2_assoc, repl_policy, INCLUSIVE, n1, n2);

    while(!fin.eof()) {
        fin >> read_write >> address;
        if(fin.eof())
            break;
        val = strtol(address, NULL, 16);

        if(read_write == 'r')
            H.read(val);
        else
            H.write(val);
    }
    H.print_statistics();
    return 0;
}
