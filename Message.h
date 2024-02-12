/*
 * @Author: Tairan Gao
 * @Date:   2024-02-10 13:47:35
 * @Last Modified by:   Tairan Gao
 * @Last Modified time: 2024-02-11 15:10:06
 */


#ifndef MESSAGE_H
#define MESSAGE_H

#include <unordered_map>
#include <memory>

#include "Utility.h"

namespace GtrTrex {

    class ItchMessage {
    public:
        ItchMessage(char message_type) : message_type(message_type) {}
        virtual ~ItchMessage() = default; // Ensure proper cleanup of derived classes
        void initalize(const std::byte*& data) {
            stock_id = read<uint16_t>(data);
            skipByOffset(data, 2);
            message_time = readTimeStamp(data);
        }

        char message_type;
        uint16_t stock_id = 0;
        uint64_t message_time = 0u;

        inline uint8_t getMsgHour() const {
            return message_time / 3600000000000LL;
        }
    };

    // R
    class StockDirectoryMessage : public ItchMessage {
    public:
        std::string stock_symbol;
        StockDirectoryMessage(const std::byte*& data) : ItchMessage('R') {
            initalize(data);
            stock_symbol = readStock(data);
            skipByOffset(data, 20);
        }
    };

    //A
    class AddOrderMessage : public ItchMessage {
    public:
        uint64_t order_id;          // Order Reference Number
        uint32_t shares;            // Shares
        uint32_t price;             // Price
        AddOrderMessage(const std::byte*& data) : ItchMessage('A') {
            initalize(data);
            order_id = read<uint64_t>(data);
            skipByOffset(data, 1);
            shares = read<uint32_t>(data);
            skipByOffset(data, 8);
            price = read<uint32_t>(data);
        }

    };

    //F
    class AddOrderMPIDAttributionMessage : public ItchMessage {
    public:
        uint64_t order_id;              // Order Reference Number
        uint64_t shares;                // Shares
        uint32_t price;                 // Price

        AddOrderMPIDAttributionMessage(const std::byte*& data) : ItchMessage('F') {
            initalize(data);
            order_id = read<uint64_t>(data);
            skipByOffset(data, 1);
            shares = read<uint32_t>(data);
            skipByOffset(data, 8);
            price = read<uint32_t>(data);
            skipByOffset(data, 4);
        }

    };

    //E
    class OrderExecutedMessage : public ItchMessage {
    public:
        uint64_t order_id;         // Order Reference Number
        uint32_t executed_shares;  // Executed Shares
        uint64_t match_number;          // Match Number

        OrderExecutedMessage(const std::byte*& data) : ItchMessage('E') {
            initalize(data);
            order_id = read<uint64_t>(data);
            executed_shares = read<uint32_t>(data);
            match_number = read<uint64_t>(data);
        }
    };


    //C
    class OrderExecutedWithPriceMessage : public ItchMessage {
    public:
        uint64_t order_id;              // Order Reference Number
        uint32_t executed_shares;       // Executed Shares
        char printable;                 // Printable
        uint32_t execution_price;       // Execution Price
        uint64_t match_number;          // Match Number

        OrderExecutedWithPriceMessage(const std::byte*& data) : ItchMessage('C') {
            initalize(data);
            order_id = read<uint64_t>(data);
            executed_shares = read<uint32_t>(data);
            match_number = read<uint64_t>(data);
            printable = read<char>(data);
            execution_price = read<uint32_t>(data);
        }
    };


    // U
    class OrderReplaceMessage : public ItchMessage {
    public:
        uint64_t original_order_id;    // Original Order Reference Number
        uint64_t new_order_id;         // New Order Reference Number
        uint32_t shares;             // Shares
        uint32_t price;              // Price

        OrderReplaceMessage(const std::byte*& data) : ItchMessage('U') {
            initalize(data);
            original_order_id = read<uint64_t>(data);
            new_order_id = read<uint64_t>(data);
            shares = read<uint32_t>(data);
            price = read<uint32_t>(data);
        }
    };


    // P
    //Trade Message (Non-Cross)
    class NonCrossTradeMessage : public ItchMessage {
    public:
        uint64_t order_id;              // Order Reference Number
        uint32_t shares;                // Shares
        uint32_t price;                 // Price
        uint64_t match_number;          // Match Number

        NonCrossTradeMessage(const std::byte*& data) : ItchMessage('P') {
            initalize(data);
            order_id = read<uint64_t>(data);
            skipByOffset(data, 1);
            shares = read<uint32_t>(data);
            skipByOffset(data, 8);
            price = read<uint32_t>(data);
            match_number = read<uint64_t>(data);
        }
    };

    // Q
    class CrossTradeMessage : public ItchMessage {
    public:

        uint64_t shares;                // Shares
        uint32_t cross_price;           // Price
        uint64_t match_number;          // Match Number

        CrossTradeMessage(const std::byte*& data) : ItchMessage('Q') {
            initalize(data);

            shares = read<uint64_t>(data);
            skipByOffset(data, 8);
            cross_price = read<uint32_t>(data);
            match_number = read<uint64_t>(data);
            skipByOffset(data, 1);
        }
    };


    // B
    class BrokenTradeMessage : public ItchMessage {
    public:
        uint64_t match_number;          // Match Number
        BrokenTradeMessage(const std::byte*& data) : ItchMessage('B') {
            initalize(data);
            match_number = read<uint64_t>(data);
        }
    };


    // Factory class to create instances of message types
    class MessageFactory {
    public:
        using MessageCreator = std::unique_ptr<ItchMessage>(*)(const std::byte*&);


        static void registerMessageCreator(char messageType, MessageCreator creator) {
            messageCreators_[messageType] = std::move(creator);
        }


        static std::unique_ptr<ItchMessage> createMessage(char messageType, const std::byte*& data) {
            auto it = messageCreators_.find(messageType);
            if (it != messageCreators_.end()) {
                return (it->second)(data);
            }
            return nullptr;
        }


    private:
        static std::unordered_map<char, MessageCreator> messageCreators_;
        static std::unordered_map<char, int> messageSizes_;

    public:
        static void populateMessageSizeMap() {
            /*--------------------------------------- VWAP related--------------------------------------*/
            messageSizes_['R'] = 38;    // Stock Directory
            messageSizes_['A'] = 35;    // Added orders
            messageSizes_['F'] = 39;    // Added orders
            messageSizes_['E'] = 30;    // Executed orders
            messageSizes_['C'] = 35;    // Executed orders
            messageSizes_['U'] = 34;    // Modifications
            messageSizes_['P'] = 43;    // Undisplayable non-cross orders executed
            messageSizes_['Q'] = 39;    // Cross Trade Message
            messageSizes_['B'] = 18;    // Broken Trade / Order ExecutionMessage
            /*-------------------------------------Not VWAP related------------------------------------*/
            messageSizes_['S'] = 11;    // System Event Message
            messageSizes_['H'] = 24;    // Stock Trading Action
            messageSizes_['Y'] = 19;    // Reg SHO Short Sale Price Test RestrictedIndicator
            messageSizes_['L'] = 25;    // Market Participant Position
            messageSizes_['V'] = 34;    // MWCB Decline Level Message
            messageSizes_['W'] = 11;    // MWCB Status Message
            messageSizes_['K'] = 27;    // Quoting Period Update
            messageSizes_['J'] = 34;    // Limit Up – Limit Down (LULD) Auction Collar
            messageSizes_['h'] = 20;    // Operational Halt
            messageSizes_['X'] = 22;    // Order Cancel Message
            messageSizes_['D'] = 18;    // Order Delete Message: Not processed but could create memory issue
            messageSizes_['I'] = 49;    //  Net Order Imbalance Indicator (NOII)Message
            messageSizes_['N'] = 19;    // Retail Price Improvement Indicator(RPII)
            messageSizes_['O'] = 47;    //Direct Listing with Capital Raise Price Discovery Message
        }

        static size_t getMessageSize(const char& messageType) {
            auto it = messageSizes_.find(messageType);
            if (it == MessageFactory::messageSizes_.end()) {
                return 0;
            }
            else {
                return it->second;
            }
        }

    };

    std::unordered_map<char, MessageFactory::MessageCreator> MessageFactory::messageCreators_;
    std::unordered_map<char, int> MessageFactory::messageSizes_;

    void registerMessageCreators() {
        MessageFactory::registerMessageCreator('R', +[](const std::byte*& data) -> std::unique_ptr<ItchMessage> {
            return std::make_unique<StockDirectoryMessage>(data);
            });
        MessageFactory::registerMessageCreator('A', +[](const std::byte*& data) -> std::unique_ptr<ItchMessage> {
            return std::make_unique<AddOrderMessage>(data);
            });
        MessageFactory::registerMessageCreator('F', +[](const std::byte*& data) -> std::unique_ptr<ItchMessage> {
            return std::make_unique<AddOrderMPIDAttributionMessage>(data);
            });
        MessageFactory::registerMessageCreator('E', +[](const std::byte*& data) -> std::unique_ptr<ItchMessage> {
            return std::make_unique<OrderExecutedMessage>(data);
            });
        MessageFactory::registerMessageCreator('C', +[](const std::byte*& data) -> std::unique_ptr<ItchMessage> {
            return std::make_unique<OrderExecutedWithPriceMessage>(data);
            });
        MessageFactory::registerMessageCreator('U', +[](const std::byte*& data) -> std::unique_ptr<ItchMessage> {
            return std::make_unique<OrderReplaceMessage>(data);
            });
        MessageFactory::registerMessageCreator('P', +[](const std::byte*& data) -> std::unique_ptr<ItchMessage> {
            return std::make_unique<NonCrossTradeMessage>(data);
            });
        MessageFactory::registerMessageCreator('Q', +[](const std::byte*& data) -> std::unique_ptr<ItchMessage> {
            return std::make_unique<CrossTradeMessage>(data);
            });
        MessageFactory::registerMessageCreator('B', +[](const std::byte*& data) -> std::unique_ptr<ItchMessage> {
            return std::make_unique<BrokenTradeMessage>(data);
            });
    }
}

#endif


