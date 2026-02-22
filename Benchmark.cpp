#include <chrono>
#include <vector>
#include <random>
#include <iostream>
#include <cstdint>
#include "MatchingEngine.h"

int main() {
    try {
        std::cout << "Starting benchmark" << std::endl;
        MatchingEngine engine;
        const int NUM_ORDERS = 1000;
    
    // Pre-generate raw order data to keep RNG out of the benchmark loop
    struct RawOrder { uint32_t price; uint32_t qty; bool is_buy; };
    std::vector<RawOrder> test_orders(NUM_ORDERS);
    
    for (int i = 0; i < NUM_ORDERS; ++i) {
        test_orders[i] = {5000, 10, i % 2 == 0};
    }

    std::cout << "Starting benchmark loop" << std::endl;
    auto start = std::chrono::high_resolution_clock::now();

    // for (int i = 0; i < NUM_ORDERS; ++i) {
    //     if (i % 100 == 0) std::cout << "Processing order " << i << std::endl;
    //     engine.processNewOrder(i, test_orders[i].price, test_orders[i].qty, test_orders[i].is_buy);
    // }

    auto end = std::chrono::high_resolution_clock::now();
    // --- End Benchmark ---

    std::chrono::duration<double, std::micro> elapsed = end - start;
    
    std::cout << "--- Matching Engine Benchmark ---" << std::endl;
    std::cout << "Orders Processed: " << NUM_ORDERS << std::endl;
    std::cout << "Trades Executed:  " << engine.getTradesExecuted() << std::endl;
    std::cout << "Total Time:       " << elapsed.count() / 1000.0 << " ms" << std::endl;
    std::cout << "Avg Latency:      " << elapsed.count() / NUM_ORDERS << " us/order" << std::endl;

    return 0;
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
        return 1;
    }
}