#include <iostream>
#include "OrderBook.h"
#include "Types.h"

class MatchingEngine {
private:
    OrderBook book_;
    OrderPool pool_;
    uint64_t trades_executed_ = 0;

public:
    void processNewOrder(uint64_t id, uint32_t price, uint32_t qty, bool is_buy) {
        Order* inbound = pool_.allocate(id, price, qty, is_buy);

        if (is_buy) {
            matchBuyOrder(inbound);
        } else {
            matchSellOrder(inbound);
        }

        // If not fully filled, add to the book
        if (inbound->qty > 0) {
            book_.addOrder(inbound);
        } else {
            pool_.deallocate(inbound);
        }
    }

    uint64_t getTradesExecuted() const { return trades_executed_; }

private:
    void matchBuyOrder(Order* inbound) {
        while (inbound->qty > 0 && book_.best_ask_ <= inbound->price) {
            PriceLevel& best_ask_level = book_.asks_[book_.best_ask_];
            Order* resting = best_ask_level.head;

            if (!resting) {
                book_.updateBestAsk();
                continue;
            }

            executeTrade(inbound, resting, best_ask_level, book_.best_ask_);
        }
    }

    void matchSellOrder(Order* inbound) {
        while (inbound->qty > 0 && book_.best_bid_ >= inbound->price) {
            PriceLevel& best_bid_level = book_.bids_[book_.best_bid_];
            Order* resting = best_bid_level.head;

            if (!resting) {
                book_.updateBestBid();
                continue;
            }

            executeTrade(inbound, resting, best_bid_level, book_.best_bid_);
        }
    }

    void executeTrade(Order* inbound, Order* resting, PriceLevel& level, uint32_t fill_price) {
        uint32_t traded_qty = std::min(inbound->qty, resting->qty);
        
        inbound->qty -= traded_qty;
        resting->qty -= traded_qty;
        trades_executed_++;

        // Partial fill handling: If resting order is filled, remove and deallocate
        if (resting->qty == 0) {
            level.pop_front();
            if (!level.head) {
                if (fill_price == book_.best_ask_) {
                    book_.updateBestAsk();
                } else if (fill_price == book_.best_bid_) {
                    book_.updateBestBid();
                }
            }
            pool_.deallocate(resting);
        }
    }
};