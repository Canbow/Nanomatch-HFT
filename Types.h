#ifndef TYPES_H
#define TYPES_H

#include <array>
#include <cstdint>
#include <stdexcept>

constexpr size_t MAX_ORDERS = 1000000;
constexpr uint32_t MAX_PRICE_TICKS = 10000;

struct Order {
    uint64_t id;
    uint32_t price;
    uint32_t qty;
    bool is_buy;
    
    // Intrusive linked list pointers for O(1) removal
    Order* prev = nullptr;
    Order* next = nullptr;
};

// Zero-allocation object pool
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
        order->id = id;
        order->price = price;
        order->qty = qty;
        order->is_buy = is_buy;
        order->prev = nullptr;
        order->next = nullptr;
        return order;
    }

    void deallocate(Order* order) {
        size_t idx = std::distance(pool_.data(), order);
        free_list_[free_idx_++] = idx;
    }
};

#endif