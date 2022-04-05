#include <iostream>
#include <chrono>
#include <string>
#include <iomanip>

#include "../BlockStoreClient.cpp"

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "usage: " << argv[0]
                    << " <num of runs, int> <address, int>\n";
        exit(1);
    }
    int num_run = atoi(argv[1]);
    int64_t addr = atoi(argv[2]);
    BlockStoreClient hadev(0, false);
    std::cout << std::setw(9);

    std::cout << "Write Performance:" << std::endl;
    std::chrono::duration<double> total_time_diff(0);
    for (int i = 0; i < num_run; ++i) {
        std::string data(4096, 'a');
        auto start = std::chrono::system_clock::now();
        int result = hadev.Write(addr, data);
        auto end = std::chrono::system_clock::now();
        total_time_diff += end - start;
    }
    std::cout << "  avg: " << total_time_diff.count() / num_run << " s\n";

    std::cout << "Read Performance:" << std::endl;
    total_time_diff -= total_time_diff;
    for (int i = 0; i < num_run; ++i) {
        std::string data(4096, 'a');
        auto start = std::chrono::system_clock::now();
        hadev.Read(addr);
        auto end = std::chrono::system_clock::now();
        total_time_diff += end - start;
    }
    std::cout << "  avg: " << total_time_diff.count() / num_run << " s\n";

    return 0;
}