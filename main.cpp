/*
 * @Author: Tairan Gao
 * @Date:   2024-02-10 12:22:36
 * @Last Modified by:   Tairan Gao
 * @Last Modified time: 2024-02-13 12:48:21
 */

#include <iostream>
#include <iomanip>
#include <fstream>

#include <bit>
#include <unordered_map>
#include <map>
#include <chrono>
#include <thread>
#include <array>

#include "Message.h"
#include "ThreadSafeQueue.h"
#include "MemoryMappedFileReader.h"

using namespace GuG;

void readDataIntoQueue(MemoryMappedFileReader& reader, ThreadSafeQueue<std::unique_ptr<ItchMessage>>& queue)
{

    const std::byte* buffer = reinterpret_cast<std::byte*>(reader.data());
    char msg_type;
    size_t message_size = 0u;
    size_t byte_read = 0u;
    size_t byte_read_update = 0u;
    const std::uint64_t MB = 1ULL << 20;             // 1 MB in bytes
    const std::uint64_t update_threshold = 100 * MB; // 1 GB in bytes

    while (byte_read < reader.size())
    {

        if (byte_read_update >= update_threshold)
        {
            std::cout << byte_read / MB << " MB parsed\n";
            byte_read_update = byte_read_update % update_threshold;
        }

        msg_type = read<char>(buffer);
        message_size = MessageFactory::getMessageSize(msg_type);
        if (message_size != 0)
        {
            auto message = MessageFactory::createMessage(msg_type, buffer);
            if (message == nullptr)
            {
                skipByOffset(buffer, message_size);
            }
            else
            {
                queue.push(std::move(message));
            }
        }
        byte_read += message_size + 1;
        byte_read_update += message_size + 1;
    }
    queue.finish();
    std::cout << "Finished Reading Data\n";
}

void calcAndOutputVWAP(uint8_t hour,
    const std::map<uint16_t, std::string>& stock_map,
    const std::unordered_map<uint16_t, std::array<uint64_t, 24>>& volume_map,
    const std::unordered_map<uint16_t, std::array<uint64_t, 24>>& dollar_volume_map,
    std::ostream& out = std::cout)
{
    for (const auto& [stock_id, stock_symbol] : stock_map)
    {
        uint64_t volume = volume_map.at(stock_id)[hour];
        uint64_t dollar_volume = dollar_volume_map.at(stock_id)[hour];
        if (volume == 0)
        {
            continue;
        }
        double vwap = dollar_volume / 10000.0 / volume;

        out << std::left << stock_symbol << ","
            << stock_id << ","
            << static_cast<int>(hour) << ","
            << std::fixed << std::setprecision(4) << vwap
            << "\n";
    }
    std::cout << "Finished Processing Data of Hour: " << static_cast<unsigned int>(hour) << "\n";
}

void processMessage(ThreadSafeQueue<std::unique_ptr<ItchMessage>>& queue, std::ostream& out = std::cout)
{
    std::unique_ptr<ItchMessage> message;
    std::map<uint16_t, std::string> stock_map;
    std::unordered_map<uint64_t, uint32_t> order_price_map;
    std::unordered_map<uint16_t, std::array<uint64_t, 24>> volume_map;
    std::unordered_map<uint16_t, std::array<uint64_t, 24>> dollar_volume_map;

    // match id : [stock_id, price, volume, time]
    std::unordered_map<uint16_t, std::tuple<uint64_t, uint32_t, uint64_t, uint8_t>> matchID_trade_map;

    uint8_t cur_hour = 0u;
    uint8_t msg_hour = 0u;

    while (true)
    {
        if (!queue.pop(message))
        {
            if (queue.isFinished())
            {
                break;
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
        }

        char message_type = message->message_type;
        msg_hour = message->getMsgHour();

        while (cur_hour < msg_hour)
        { // leave time for fixing broken message
            calcAndOutputVWAP(cur_hour, stock_map, volume_map, dollar_volume_map, out);
            ++cur_hour;
        }
        switch (message_type)
        {
        case 'R':
        {
            auto casted_msg = static_cast<StockDirectoryMessage*>(message.get());
            stock_map[casted_msg->stock_id] = casted_msg->stock_symbol;
            volume_map[casted_msg->stock_id] = std::array<uint64_t, 24>{};
            dollar_volume_map[casted_msg->stock_id] = std::array<uint64_t, 24>{};
            break;
        }
        case 'A':
        {
            auto casted_msg = static_cast<AddOrderMessage*>(message.get());
            order_price_map[casted_msg->order_id] = casted_msg->price;
            break;
        }
        case 'F':
        {
            auto casted_msg = static_cast<AddOrderMPIDAttributionMessage*>(message.get());
            order_price_map[casted_msg->order_id] = casted_msg->price;
            break;
        }
        case 'E':
        { // Actual Trade
            auto casted_msg = static_cast<OrderExecutedMessage*>(message.get());
            uint32_t cur_price = order_price_map[casted_msg->order_id];
            uint32_t cur_volume = casted_msg->executed_shares;

            matchID_trade_map[casted_msg->match_number] = std::make_tuple(casted_msg->stock_id, cur_price, cur_volume, msg_hour);
            dollar_volume_map[casted_msg->stock_id][msg_hour] += (static_cast<uint64_t>(cur_price) * cur_volume);
            volume_map[casted_msg->stock_id][msg_hour] += static_cast<uint64_t>(cur_volume);
            break;
        }
        case 'C':
        {
            auto casted_msg = static_cast<OrderExecutedWithPriceMessage*>(message.get());
            if (casted_msg->printable == 'N')
            {
                // Only count Printable
                continue;
            }
            uint32_t cur_price = casted_msg->execution_price;
            uint32_t cur_volume = casted_msg->executed_shares;

            matchID_trade_map[casted_msg->match_number] = std::make_tuple(casted_msg->stock_id, cur_price, cur_volume, msg_hour);
            dollar_volume_map[casted_msg->stock_id][msg_hour] += (static_cast<uint64_t>(cur_price) * cur_volume);
            volume_map[casted_msg->stock_id][msg_hour] += static_cast<uint64_t>(cur_volume);
            break;
        }
        case 'U':
        {
            auto casted_msg = static_cast<OrderReplaceMessage*>(message.get());
            order_price_map.erase(casted_msg->original_order_id);
            order_price_map[casted_msg->new_order_id] = casted_msg->price;
            break;
        }
        case 'P':
        {
            auto casted_msg = static_cast<NonCrossTradeMessage*>(message.get());
            uint32_t cur_price = casted_msg->price;
            uint32_t cur_volume = casted_msg->shares;

            matchID_trade_map[casted_msg->match_number] = std::make_tuple(casted_msg->stock_id, cur_price, cur_volume, msg_hour);
            dollar_volume_map[casted_msg->stock_id][msg_hour] += (static_cast<uint64_t>(cur_price) * cur_volume);
            volume_map[casted_msg->stock_id][msg_hour] += static_cast<uint64_t>(cur_volume);
            break;
        }
        case 'Q':
        {
            auto casted_msg = static_cast<CrossTradeMessage*>(message.get());
            uint32_t cur_price = casted_msg->cross_price;
            uint64_t cur_volume = casted_msg->shares;

            matchID_trade_map[casted_msg->match_number] = std::make_tuple(casted_msg->stock_id, cur_price, cur_volume, msg_hour);
            dollar_volume_map[casted_msg->stock_id][msg_hour] += (static_cast<uint64_t>(cur_price) * cur_volume);
            volume_map[casted_msg->stock_id][msg_hour] += static_cast<uint64_t>(cur_volume);
            break;
        }
        case 'B':
        {
            auto casted_msg = static_cast<BrokenTradeMessage*>(message.get());
            uint64_t match_number = casted_msg->match_number;

            auto it = matchID_trade_map.find(casted_msg->match_number);
            if (it == matchID_trade_map.end())
            {
                continue;
            }
            auto [stock_id, cur_price, cur_volume, trade_hour] = it->second;
            dollar_volume_map[stock_id][trade_hour] -= (static_cast<uint64_t>(cur_price) * cur_volume);
            volume_map[stock_id][trade_hour] -= static_cast<uint64_t>(cur_volume);
            break;
        }
        default:
            continue;
        }
    }
    while (cur_hour < 24u)
    {
        calcAndOutputVWAP(cur_hour, stock_map, volume_map, dollar_volume_map, out);
        ++cur_hour;
    }
}

int main(int argc, char* argv[])
{
    const char* file_path;
    if (argc < 2)
    {
        file_path = "01302019.NASDAQ_ITCH50";
    }
    else
    {
        file_path = argv[1];
    }

    MemoryMappedFileReader fileReader(file_path);
    ThreadSafeQueue<std::unique_ptr<ItchMessage>> queue;

    registerMessageCreators();
    MessageFactory::populateMessageSizeMap();

    std::thread reader_thread(readDataIntoQueue, std::ref(fileReader), std::ref(queue));

    std::ofstream file_stream("output.csv", std::ios::out | std::ios::trunc);
    if (!file_stream.is_open())
    {
        std::cerr << "Failed to open file" << std::endl;
        return 1;
    }
    file_stream << "STOCK_SYMBOL"
        << ","
        << "STOCK_ID"
        << ","
        << "HOUR_AFTER_MIDNIGHT"
        << ","
        << "VWAP" << std::endl;

    std::thread process_thread(processMessage, std::ref(queue), std::ref(file_stream));
    std::cout << "VWAP Job Finished \n";
    // Join threads
    reader_thread.join();
    process_thread.join();


    return 0;
}
