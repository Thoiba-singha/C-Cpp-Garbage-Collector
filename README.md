# 🗑️ Realtime C/C++ Garbage Collector

A lightweight, thread safe garbage collector (GC)** written in pure C++.  

The GC is designed to be:  
- **Automatic** → cleanup followed by RAII principles. 
- **GC::Ptr** → with inbuit cyclic ref safe without weak_ptr.  
- **Safe** → prevents double frees and dangling pointers (only in C++).  
- **C++ APIS** → (`GC::Ptr`, `GC::New`, `Ref`).
- **C APIS** → (`gc_calloc`, `gc_malloc`, `gc_new`, `gc_new_array`, `Ptr`).  
- **Realloc** → Not supported in Both Languages.  

---

- **Allocation APIs in C**  
- `gc_malloc` → allocate managed memory.  
- `gc_calloc` → zero-initialized allocation.  
- `gc_new_array_` → array.
- `gc_new` → single object.

---

- **Allocation APIs in C++**  
- `GC::Ptr` → similar to std::shared_ptr with inbuild cyclic ref safety.  
- `GC::New` → similar to std::make_shared().  
- `ref_count` → to count the current ref.
- `Ref` → Cyclic ref safe(No need weak_ptr).

---

**C - Example usage:**
```c
#include "gc/gc.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    int x;
    float y;
    struct Node* next;
} Node;

int main() {

    Node* n1 = gc_malloc(sizeof(Node));
    Node* n2 = gc_malloc(sizeof(Node));

    // Create a cycle
    n1->next = n2;
    n2->next = n1;

    printf("Cycle created:\n");
    printf("  n1->next = %p\n", (void*)n1->next);
    printf("  n2->next = %p\n", (void*)n2->next);

    //// Break the cycle FIRST
    n2->next = NULL;
    n1->next = NULL;

    // Debug: verify pointers are NULL
    printf("After break:\n");
    printf("  n1->next = %p (should be NULL)\n", (void*)n1->next);
    printf("  n2->next = %p (should be NULL)\n", (void*)n2->next);

} 


```

**C++ - Example usage:**
```c


#include "gc/gc.h"
#include <iostream>

struct Node {
    int data;
    GC::Ptr<Node> next;

    explicit Node(int d) : data(d) {
        std::cout << "Node(" << data << ") created\n";
    }

    ~Node() {
        std::cout << "Node(" << data << ") destroyed\n";
    }
};

class CarDriver {
public:
    void Drive(const std::string& name) {
        std::lock_guard<std::mutex> lock(mtx);

        std::cout << name << " driving\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        std::cout << name << " driving completed\n";
    }

private:
    std::mutex mtx;
};

int main() {


    //  Basic cyclic dependency test
    {
        GC::Ptr<Node> node1(new Node(40));
        GC::Ptr<Node> node2(new Node(50));
        node1->next.Ref(node2);
        node2->next.Ref(node1);
        std::cout << "Node1 use_count: " << node1.ref_count() << "\n\n";
        std::cout << "Node2 use_count: " << node2.ref_count() << "\n\n";

    }
   

    // Race condition test
    /*
    {
        GC::Ptr<CarDriver> driver = GC::New<CarDriver>();

        GC::Ptr<CarDriver> weakDriver = driver;

        std::vector<std::thread> threads;

        for (int i = 0; i < 5; ++i) {
            threads.emplace_back([weakDriver, i]() mutable {

                // Convert weak_ptr → shared_ptr safely
                if (auto shared = weakDriver.lock()) {
                    shared->Drive("Rahul " + std::to_string(i));
                }
                else {
                    // Object destroyed, skip work
                    std::cout << "Driver no longer exists.\n";
                }
                });
        }

        // Join threads
        for (auto& t : threads)
            t.join();
    }
    */
    return 0;
}



```

