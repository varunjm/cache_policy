#include <iostream>
#include <fstream>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <list>

#define ADDRESS_WIDTH 48
#define DEBUG 0
using namespace std;

enum assoc{DIRECT=1, TWO_WAY, FOUR_WAY=4, EIGHT_WAY=8};
enum rep_p{LRU, FIFO};
enum rw{READ=0, WRITE=1};
enum incls{NINCLUSIVE=0, INCLUSIVE, EXCLUSIVE};

struct FLAGS{
    long evicted_address;
    bool dirty;
    bool evicted;
    bool miss;
};

struct TAG {
    long value;
    bool dirty;
    TAG() {}
    bool operator == ( const TAG& rhs ) {
        return (value == rhs.value && dirty == rhs.dirty);
    }
};

typedef list<TAG>::iterator iter;

FLAGS * get_flags_node() {
    FLAGS *temp = (FLAGS *)malloc(sizeof(FLAGS));
    temp->dirty = temp->evicted = temp->miss = false;
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

    list<TAG> *memory;

    long get_address(long tag, long index) {
        return (((tag << index_width) + index) << block_offset_width);
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
        memory = new list<TAG>[set_count];
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

    FLAGS * read(long address) {           // returns address of evicted block, else returns -1
        long index = (address >> block_offset_width) % (1 << index_width); // set number
        long tag = (address >> (block_offset_width + index_width)); // value stored
        bool hit = false;
        FLAGS *temp = get_flags_node();
        TAG tag2;
        int i;
        iter it;

        read_counter++;
        for (it = memory[index].begin(); it != memory[index].end(); ++it) {
            if( (*it).value == tag ) {
                hit = true;
                tag2 = *it;
                break;
            }
        }
        if(hit) {
            read_hit++;
            if(repl_policy == LRU) {
                memory[index].remove(tag2);
                memory[index].push_back(tag2);     // Change to most recently used position
            }
        }
        else {
            read_miss++;
            temp->miss = true;
            if(memory[index].size() == associativity) { // Remove front if list full
                tag2 = memory[index].front();
                memory[index].pop_front();
                if(tag2.dirty) {
                    write_back++;
                    temp->dirty = true;
                }
                temp->evicted = true;
                temp->evicted_address = get_address(tag2.value, index);
            }
            tag2.value = tag;
            tag2.dirty = false;
            memory[index].push_back(tag2);      // Insert new tag
        }
        if(DEBUG) {
            cout << name << " " << READ << " " << index <<  " " << tag << " " << hit << endl;
        }
        return temp;
    }

    FLAGS * write(long address) {           // returns address of evicted block, else returns -1
        long index = (address >> block_offset_width) % (1 << index_width); // set number
        long tag = (address >> (block_offset_width + index_width)); // value stored
        bool hit = false;
        FLAGS *temp = get_flags_node();
        TAG tag2;
        int i;
        iter it;

        write_counter++;
        for (it = memory[index].begin(); it != memory[index].end(); ++it) {
            if( (*it).value == tag ) {
                hit = true;
                (*it).dirty = true;                 // set dirty bit if hit
                tag2 = *it;
                break;
            }
        }
        if(hit) {
            write_hit++;
            if(repl_policy == LRU) {
                memory[index].remove(tag2);
                memory[index].push_back(tag2);      // update LRU order
            }
        }
        else {
            write_miss++;
            temp->miss = true;
            if(memory[index].size() == associativity) {
                tag2 = memory[index].front();
                memory[index].pop_front();      // Remove front if list full
                if(tag2.dirty) {
                    write_back++;
                    temp->dirty = true;
                }
                temp->evicted = true;
                temp->evicted_address = get_address(tag2.value, index);
            }
            tag2.value = tag;
            tag2.dirty = true;
            memory[index].push_back(tag2);      // Insert new tag
        }
        if(DEBUG) {
            cout << name << " " << READ << " " << index <<  " " << tag << " " << hit << endl;
        }
        return temp;
    }

    void evict(long address) {
        long index = (address >> block_offset_width) % (1 << index_width); // set number
        long tag = (address >> (block_offset_width + index_width)); // value stored
        TAG temp;
        bool flag = false;

        for(iter it = memory[index].begin(); it != memory[index].end(); ++it) {
            if((*it).value == tag) {
                temp = (*it);
                flag = true;
                break;
            }
        }
        if(flag) {
            if(temp.dirty)
                write_back++;
            memory[index].remove(temp);
        }
    }
};

class hierarchy {
    cache *L1;
    cache *L2;

    int inclusion;

    public:
    hierarchy(int b, int l1_size, int l1_assoc, int l2_size, int l2_assoc, bool rp, int incl, char n1[], char n2[]) {
        L1 = new cache(b, l1_size, l1_assoc, rp, n1);
        L2 = new cache(b, l2_size, l2_assoc, rp, n2);
        inclusion = incl;
    }

    hierarchy(int b, int l1_size, int l1_assoc, bool rp, char n1[]) {
        L1 = new cache(b, l1_size, l1_assoc, rp, n1);
        inclusion = NINCLUSIVE;
    }
    void print_statistics() {
        cout << "L1 : \n";
        L1->print_statistics();
        if(inclusion != NINCLUSIVE) {
            cout << "\nL2 : \n";
            L2->print_statistics();
        }
    }

    void read(long address) {
        FLAGS * temp1, * temp2;

        switch(inclusion) {
            case INCLUSIVE: {
                temp1 = L1->read(address);
                if(temp1->miss) {
                    // This order of statements is important:
                    // 1.First L2 reads the missed block
                    // 2.if there is a replacement,
                    //  the replaced block from L2 is also removed from L1 (if it exists there)
                    // 2. Then an old block (possibly dirty) is replaced in L1
                    // 3. This replaced block if dirty is written back to L2
                    temp2 = L2->read(address);
                    if(temp2->evicted) {
                        L1->evict(temp2->evicted_address);
                    }
                    if(temp1->dirty) {
                        L2->write(temp1->evicted_address);
                    }
                    free(temp2);
                    free(temp1);
                }
                break;
            }
            case NINCLUSIVE: {
                L1->read(address);
                break;
            }
            case EXCLUSIVE: {
                break;
            }
        }
    }

    void write(long address) {
        FLAGS * temp1, * temp2;

        switch(inclusion) {
            case INCLUSIVE: {
                temp1 = L1->write(address);
                if(temp1->miss) {
                    // This order of statements is important:
                    // 1.First L2 reads the missed block
                    // 2.if there is a replacement,
                    //  the replaced block from L2 is also removed from L1 (if it exists there)
                    // 2. Then an old block (possibly dirty) is replaced in L1
                    // 3. This replaced block if dirty is written back to L2
                    temp2 = L2->read(address);
                    if(temp2->evicted) {
                        L1->evict(temp2->evicted_address);
                    }
                    if(temp1->dirty) {
                        L2->write(temp1->evicted_address);
                    }
                    free(temp2);
                    free(temp1);
                }
                break;
            }
            case NINCLUSIVE: {
                L1->write(address);
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
    char trace_file[20] = "./Traces/GCC.t";
    // char trace_file[20] = "test1.txt";
    // int inclusion =

    char read_write, address[20];
    long val;
    char n1[20] = "L1", n2[20] = "L2";
    ifstream fin(trace_file, ifstream::binary);
    hierarchy * H;

    if(l2_size == 0) {
        H = new hierarchy(block_size, l1_size, l1_assoc, repl_policy, n1);
    } else {
        H = new hierarchy(block_size, l1_size, l1_assoc, l2_size, l2_assoc, repl_policy, INCLUSIVE, n1, n2);
    }
    while(!fin.eof()) {
        fin >> read_write >> address;
        if(fin.eof())
            break;
        val = strtol(address, NULL, 16);

        if(read_write == 'r')
            H->read(val);
        else
            H->write(val);
    }
    H->print_statistics();
    return 0;
}
