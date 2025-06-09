#include <iostream>
#include <cstdint>
#include <cstddef>
#include <type_traits>
#include <cstring>
#include <vector>

class cma{
private:
    size_t totalAllocated = 0;
    size_t totalFreed = 0;
    size_t numAllocations = 0;
    size_t numFrees = 0;

    static const size_t HEAP_SIZE = 1024 * 1024;

    struct block{
        size_t size;
        bool free;
        block* next;
    };
    alignas(std::max_align_t) uint8_t heap[HEAP_SIZE];
    block* freeList;
    
public:
    cma(){
        freeList = reinterpret_cast<block*>(heap);
        freeList->size = HEAP_SIZE - sizeof(block);
        freeList->free = true;
        freeList->next = nullptr;
    }
    void* allocate(size_t size){
        block* current = freeList;
        while(current){
            if(current->free && current->size >= size){
                if (current->size >= size + sizeof(block) + 8){
                    block* newBlock = reinterpret_cast<block*>(
                        reinterpret_cast<uint8_t*>(current) + sizeof(block) + size
                    );
                    newBlock->size = current->size - size - sizeof(block);
                    newBlock->free = true;
                    newBlock->next = current->next;

                    current->size = size;
                    current->next = newBlock;
                }
                current->free = false;
                totalAllocated += current->size;
                numAllocations++;
                return reinterpret_cast<uint8_t*>(current) + sizeof(block);
            }
            current = current->next;
        }
        return nullptr;
    }

    void deallocate(void* ptr){
        if(!ptr) return;
        block* bl = reinterpret_cast<block*>(reinterpret_cast<uint8_t*>(ptr) - sizeof(block));
        bl->free = true;
        totalFreed += bl->size;
        numFrees++;
        block* current = freeList;
        while(current && current->next){
            if(current->free && current->next->free){
                current->size += sizeof(block) + current->next->size;
                current->next = current->next->next;
            } else {
                current = current->next;
            }
        }
    }

    void printBlocks() const {
        const block* current = freeList;
        std::cout << "Block list:\n";
        while (current) {
            std::cout << "  Block at " << current << " | size: " << current->size << " | free: " << (current->free ? "yes" : "no") << " | next: " << current->next << "\n";
            current = current->next;
        }
    }
    void* reallocate(void* ptr, size_t newSize) {
        if (!ptr) return allocate(newSize);
        if (newSize == 0) {
            deallocate(ptr);
            return nullptr;
        }

        block* oldBlock = reinterpret_cast<block*>(
            reinterpret_cast<uint8_t*>(ptr) - sizeof(block)
        );

        if (oldBlock->size >= newSize) {
            return ptr;
        }

        void* newPtr = allocate(newSize);
        if (!newPtr) return nullptr;

        std::memcpy(newPtr, ptr, oldBlock->size);

        deallocate(ptr);

        return newPtr;
    }

    void printStats() const{
        std::cout << "\nALLOCATOR STATS \n";
        std::cout << "TOTAL ALLOCATED: " << totalAllocated << " bytes\n";
        std::cout << "TOTAL FREED: " << totalFreed << " bytes\n";
        std::cout << "CURRENTLY USED: " << totalAllocated - totalFreed << " bytes\n";
        std::cout << "NUMBER OF ALLOCATIONS: " << numAllocations << "\n";
        std::cout << "NUMBER OF FREES: " << numFrees << "\n";
    }
};

template<typename T>
class cma_allocator {
public:
    using value_type = T;

    static cma memory;

    cma_allocator() noexcept {}
    template<typename U> cma_allocator(const cma_allocator<U>&) noexcept {}

    T* allocate(std::size_t n) {
        void* ptr = memory.allocate(n * sizeof(T));
        if (!ptr) throw std::bad_alloc();
        return static_cast<T*>(ptr);
    }

    void deallocate(T* p, std::size_t n) noexcept {
        memory.deallocate(p);
    }

    template<typename U>
    struct rebind {
        using other = cma_allocator<U>;
    };

    bool operator==(const cma_allocator&) const noexcept { return true; }
    bool operator!=(const cma_allocator&) const noexcept { return false; }
};

template<typename T>
cma cma_allocator<T>::memory;

int main(){
    std :: cout << "----TESTING----\n";
    std::vector<int, cma_allocator<int>> allocator;
    
    allocator.push_back(4);
    allocator.push_back(16);
    allocator.push_back(6);
    allocator.push_back(90);
    allocator.push_back(7);

    std :: cout<<"VECTOR ELEMENTS: " << '\n';
    for(int it:allocator){
        std :: cout << it << "\n";
    }
    
    cma_allocator<int>::memory.printBlocks();

    cma_allocator<int>::memory.printStats();

    allocator.pop_back();
    allocator.erase(allocator.begin() + 1);
    allocator.shrink_to_fit();

    std::cout<<"Remaining elements after popping:" << '\n';
    for(int it : allocator){
        std :: cout << it << '\n';
    }

    cma_allocator<int>::memory.printBlocks();

    cma_allocator<int>::memory.printStats();

    return 0;
}