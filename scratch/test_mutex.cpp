#include <mutex>
#include <iostream>
int main() {
    std::mutex m;
    std::lock_guard<std::mutex> lock(m);
    std::cout << "OK\n";
    return 0;
}
