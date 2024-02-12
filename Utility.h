/*
 * @Author: Tairan Gao
 * @Date:   2024-02-10 15:08:25
 * @Last Modified by:   Tairan Gao
 * @Last Modified time: 2024-02-11 15:10:43
 */
#ifndef UTILITY_H
#define UTILITY_H

#include <string>
#include <cstdint>
#include <cassert>
#include <cctype>
#include <iostream>

namespace GtrTrex {
    // Helper for byte swapping
    template<typename T>
    T swap_endian(T u);

    template<>
    uint16_t swap_endian<uint16_t>(uint16_t val) {
        return __builtin_bswap16(val);
    }

    template<>
    uint32_t swap_endian<uint32_t>(uint32_t val) {
        return __builtin_bswap32(val);
    }

    template<>
    uint64_t swap_endian<uint64_t>(uint64_t val) {
        return __builtin_bswap64(val);
    }

    template<typename T>
    void skip(const std::byte*& buffer) {
        buffer += sizeof(T);
    }

    void skipByOffset(const std::byte*& buffer, std::size_t offset) {
        buffer += offset;
    }


    template<typename T>
    T read(const std::byte*& buffer, bool bigEndian = true) {
        const T* alignedBuffer = reinterpret_cast<const T*>(buffer);
        T value = *alignedBuffer;
        buffer += sizeof(T);

        if (bigEndian && !std::is_same_v<T, char>) {
            value = swap_endian(value);
        }

        return value;
    }

    // Specialization for char (no byte swapping needed)
    template<>
    char read<char>(const std::byte*& buffer, bool /* bigEndian */) {
        const char* alignedBuffer = reinterpret_cast<const char*>(buffer);
        char value = *alignedBuffer;
        ++buffer; // Move to the next byte
        return value;
    }


    std::string readStock(const std::byte*& buffer) {
        assert((reinterpret_cast<uintptr_t>(buffer) % alignof(char[8])) == 0 && "Data is misaligned");
        char value[9];
        size_t length = 0;
        auto start_buffer = buffer;
        // Iterate through the characters and stop when no further characters with values are found
        for (size_t i = 0; i < 8; ++i) {
            char ch = static_cast<char>(*buffer);
            if (ch == '\0' || std::isspace(ch)) {
                break;
            }
            value[length++] = ch;
            ++buffer;
        }
        value[length] = '\0';
        buffer = start_buffer + 8;
        return std::string(value);;
    }

    // Specialization for a 6-byte timestamp
    uint64_t readTimeStamp(const std::byte*& buffer, bool bigEndian = true) {
        uint64_t timestamp = 0;
        if (bigEndian) {
            for (int i = 0; i < 6; ++i) {
                timestamp |= (static_cast<uint64_t>(static_cast<unsigned char>(buffer[i]))) << ((5 - i) * 8);
            }
        }
        else {
            for (int i = 0; i < 6; ++i) {
                timestamp |= (static_cast<uint64_t>(static_cast<unsigned char>(buffer[i]))) << (i * 8);
            }
        }
        buffer += 6;
        return timestamp;
    }
}

#endif