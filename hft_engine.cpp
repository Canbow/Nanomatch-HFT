#include <iostream>
#include <vector>
#include <array>
#include <chrono>
#include <random>
#include <cstdint>
#include <stdexcept>
#include<memory>
constexpr size_t MAX_ORDERS = 1000000;
constexpr uint32_t MAX_PRICE_TICKS = 4096; // 64 blocks of 64 bits

// --- 1. Types & Memory Management ---
struct Order {
    uint64_t id;
    uint32_t price;
    uint32_t qty;
    bool is_buy;
    Order* prev = nullptr;
    Order* next = nullptr;
};

class OrderPool {
private:
    std::array<Order, MAX_ORDERS> pool_;
    std::array<size_t, MAX_ORDERS> free_list_;
    size_t free_idx_;

public:
    OrderPool() : free_idx_(MAX_ORDERS) {
        for (size_t i = 0; i < MAX_ORDERS; ++i) {
            free_list_[i] = MAX_ORDERS - 1 - i;
        }
    }

    Order* allocate(uint64_t id, uint32_t price, uint32_t qty, bool is_buy) {
        if (free_idx_ == 0) throw std::runtime_error("OrderPool exhausted");
        size_t idx = free_list_[--free_idx_];
        Order* order = &pool_[idx];
        order->id = id; order->price = price; order->qty = qty; order->is_buy = is_buy;
        order->prev = nullptr; order->next = nullptr;
        return order;
    }

    void deallocate(Order* order) {
        size_t idx = std::distance(pool_.data(), order);
        free_list_[free_idx_++] = idx;
    }
};

// --- 2. O(1) Hardware-Accelerated Price Tracker ---
class FastPriceTracker {
private:
    uint64_t summary_word_ = 0;
    uint64_t data_words_[64] = {0};

public:
    void setPriceLevel(uint32_t price) {
        uint32_t word_idx = price / 64;
        uint32_t bit_idx = price % 64;
        data_words_[word_idx] |= (1ULL << bit_idx);
        summary_word_ |= (1ULL << word_idx);
    }

    void clearPriceLevel(uint32_t price) {
        uint32_t word_idx = price / 64;
        uint32_t bit_idx = price % 64;
        data_words_[word_idx] &= ~(1ULL << bit_idx);
        if (data_words_[word_idx] == 0) {
            summary_word_ &= ~(1ULL << word_idx);
        }
    }

    uint32_t getBestAsk() const {
        if (summary_word_ == 0) return MAX_PRICE_TICKS; 
        uint32_t lowest_word = __builtin_ctzll(summary_word_);
        uint32_t lowest_bit = __builtin_ctzll(data_words_[lowest_word]);
        return (lowest_word * 64) + lowest_bit;
    }

    uint32_t getBestBid() const {
        if (summary_word_ == 0) return 0; 
        uint32_t highest_word = 63 - __builtin_clzll(summary_word_);
        uint32_t highest_bit = 63 - __builtin_clzll(data_words_[highest_word]);
        return (highest_word * 64) + highest_bit;
    }
};

// --- 3. Intrusive Linked List & Order Book ---
struct PriceLevel {
    Order* head = nullptr;
    Order* tail = nullptr;

    bool isEmpty() const { return head == nullptr; }

    void push_back(Order* order) {
        if (!head) { head = tail = order; } 
        else { tail->next = order; order->prev = tail; tail = order; }
    }

    Order* pop_front() {
        if (!head) return nullptr;
        Order* order = head;
        head = head->next;
        if (head) head->prev = nullptr;
        else tail = nullptr;
        return order;
    }
};

class OrderBook {
public:
    std::array<PriceLevel, MAX_PRICE_TICKS> bids_;
    std::array<PriceLevel, MAX_PRICE_TICKS> asks_;
    FastPriceTracker bid_tracker_;
    FastPriceTracker ask_tracker_;

    void addOrder(Order* order) {
        if (order->is_buy) {
            if (bids_[order->price].isEmpty()) bid_tracker_.setPriceLevel(order->price);
            bids_[order->price].push_back(order);
        } else {
            if (asks_[order->price].isEmpty()) ask_tracker_.setPriceLevel(order->price);
            asks_[order->price].push_back(order);
        }
    }
};

// --- 4. Matching Engine ---
class MatchingEngine {
private:
    OrderBook book_;
    OrderPool pool_;
    uint64_t trades_executed_ = 0;

public:
    void processNewOrder(uint64_t id, uint32_t price, uint32_t qty, bool is_buy) {
        Order* inbound = pool_.allocate(id, price, qty, is_buy);

        if (is_buy) matchBuyOrder(inbound);
        else matchSellOrder(inbound);

        if (inbound->qty > 0) book_.addOrder(inbound);
        else pool_.deallocate(inbound);
    }

    uint64_t getTradesExecuted() const { return trades_executed_; }

private:
    void matchBuyOrder(Order* inbound) {
        while (inbound->qty > 0) {
            uint32_t best_ask = book_.ask_tracker_.getBestAsk();
            if (best_ask > inbound->price || best_ask == MAX_PRICE_TICKS) break;

            PriceLevel& level = book_.asks_[best_ask];
            Order* resting = level.head;
            
            executeTrade(inbound, resting, level, best_ask, false);
        }
    }

    void matchSellOrder(Order* inbound) {
        while (inbound->qty > 0) {
            uint32_t best_bid = book_.bid_tracker_.getBestBid();
            if (best_bid < inbound->price || best_bid == 0) break;

            PriceLevel& level = book_.bids_[best_bid];
            Order* resting = level.head;

            executeTrade(inbound, resting, level, best_bid, true);
        }
    }

    void executeTrade(Order* inbound, Order* resting, PriceLevel& level, uint32_t fill_price, bool is_bid_book) {
        uint32_t traded_qty = std::min(inbound->qty, resting->qty);
        inbound->qty -= traded_qty;
        resting->qty -= traded_qty;
        trades_executed_++;

        if (resting->qty == 0) {
            level.pop_front();
            if (level.isEmpty()) {
                if (is_bid_book) book_.bid_tracker_.clearPriceLevel(fill_price);
                else book_.ask_tracker_.clearPriceLevel(fill_price);
            }
            pool_.deallocate(resting);
        }
    }
};

// --- 5. Benchmark Suite ---
int main() {
    auto engine = std::make_unique<MatchingEngine>();
    const int NUM_ORDERS = 500000;
    
    struct RawOrder { uint32_t price; uint32_t qty; bool is_buy; };
    std::vector<RawOrder> test_orders(NUM_ORDERS);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    // Tightly grouped prices to guarantee heavy trading activity
    std::uniform_int_distribution<uint32_t> price_dist(2000, 2050); 
    std::uniform_int_distribution<uint32_t> qty_dist(10, 100);
    std::uniform_int_distribution<int> side_dist(0, 1);

    for (int i = 0; i < NUM_ORDERS; ++i) {
        test_orders[i] = {price_dist(gen), qty_dist(gen), (bool)side_dist(gen)};
    }

    std::cout << "Starting matching engine benchmark..." << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_ORDERS; ++i) {
        // Notice we use engine-> now instead of engine.
        engine->processNewOrder(i, test_orders[i].price, test_orders[i].qty, test_orders[i].is_buy);
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::nano> elapsed_ns = end - start;
    std::chrono::duration<double, std::milli> elapsed_ms = end - start;
    
    std::cout << "--- Matching Engine Results ---" << std::endl;
    std::cout << "Orders Processed: " << NUM_ORDERS << std::endl;
    std::cout << "Trades Executed:  " << engine->getTradesExecuted() << std::endl;
    std::cout << "Total Time:       " << elapsed_ms.count() << " ms" << std::endl;
    std::cout << "Avg Latency:      " << elapsed_ns.count() / NUM_ORDERS << " ns/order" << std::endl;

    return 0;
}