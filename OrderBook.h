#ifndef ORDERBOOK_H
#define ORDERBOOK_H

#include <cstdint>
#include <stdexcept>
#include <array>
#include "Types.h"

// Support for 4096 price ticks (64 blocks of 64 bits)

struct Order; // Forward declaration

struct PriceLevel {
    Order* head = nullptr;
    Order* tail = nullptr;
    
    void addOrder(Order* order) {
        order->prev = tail;
        order->next = nullptr;
        if (tail) {
            tail->next = order;
        } else {
            head = order;
        }
        tail = order;
    }
    
    void pop_front() {
        if (!head) return;
        Order* old_head = head;
        head = head->next;
        if (head) {
            head->prev = nullptr;
        } else {
            tail = nullptr;
        }
        old_head->next = nullptr;
        old_head->prev = nullptr;
    }
};

class FastPriceTracker {
private:
    uint64_t summary_word_ = 0;           // 1 bit per data_word_
    uint64_t data_words_[64] = {0};       // 1 bit per price tick

public:
    // Mark a price level as active (O(1) - Bitwise OR)
    void setPriceLevel(uint32_t price) {
        uint32_t word_idx = price / 64;
        uint32_t bit_idx = price % 64;
        
        data_words_[word_idx] |= (1ULL << bit_idx);
        summary_word_ |= (1ULL << word_idx);
    }

    // Mark a price level as empty (O(1) - Bitwise AND NOT)
    void clearPriceLevel(uint32_t price) {
        uint32_t word_idx = price / 64;
        uint32_t bit_idx = price % 64;
        
        data_words_[word_idx] &= ~(1ULL << bit_idx);
        
        // If the data word is completely empty, update the summary
        if (data_words_[word_idx] == 0) {
            summary_word_ &= ~(1ULL << word_idx);
        }
    }

    // O(1) lookup for the Best Ask (Lowest active price)
    uint32_t getBestAsk() const {
        if (summary_word_ == 0) return MAX_PRICE_TICKS; // Book is empty
        
        // __builtin_ctzll counts trailing zeros (finds the LOWEST set bit)
        uint32_t lowest_active_word = __builtin_ctzll(summary_word_);
        uint32_t lowest_active_bit = __builtin_ctzll(data_words_[lowest_active_word]);
        
        return (lowest_active_word * 64) + lowest_active_bit;
    }

    // O(1) lookup for the Best Bid (Highest active price)
    uint32_t getBestBid() const {
        if (summary_word_ == 0) return 0; // Book is empty
        
        // __builtin_clzll counts leading zeros. 
        // 63 - clzll finds the HIGHEST set bit.
        uint32_t highest_active_word = 63 - __builtin_clzll(summary_word_);
        uint32_t highest_active_bit = 63 - __builtin_clzll(data_words_[highest_active_word]);
        
        return (highest_active_word * 64) + highest_active_bit;
    }
};

class OrderBook {
public:
    std::array<PriceLevel, MAX_PRICE_TICKS> asks_;
    std::array<PriceLevel, MAX_PRICE_TICKS> bids_;
    uint32_t best_ask_ = MAX_PRICE_TICKS;
    uint32_t best_bid_ = 0;

public:
    void addOrder(Order* order) {
        if (order->is_buy) {
            bids_[order->price].addOrder(order);
            if (order->price > best_bid_) {
                best_bid_ = order->price;
            }
        } else {
            asks_[order->price].addOrder(order);
            if (order->price < best_ask_) {
                best_ask_ = order->price;
            }
        }
    }

    void updateBestAsk() {
        for (uint32_t i = 0; i < MAX_PRICE_TICKS; ++i) {
            if (asks_[i].head) {
                best_ask_ = i;
                return;
            }
        }
        best_ask_ = MAX_PRICE_TICKS;
    }

    void updateBestBid() {
        for (uint32_t i = MAX_PRICE_TICKS; i > 0; --i) {
            if (bids_[i-1].head) {
                best_bid_ = i-1;
                return;
            }
        }
        best_bid_ = 0;
    }
};

#endif