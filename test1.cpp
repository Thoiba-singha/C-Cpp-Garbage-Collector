

#include "gc/gc.h"
#include <iostream>


// ========================= EXAMPLES =========================


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

