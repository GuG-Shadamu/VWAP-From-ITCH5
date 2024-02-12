# ITCH 5.0 Data Processor for VWAP Calculation

### Tairan Gao

## Overview

This project provides a C++ application designed to read ITCH 5.0 format stock market data, process the trades, and calculate the Volume Weighted Average Price (VWAP) for each stock listed on an exchange for every hour of trading. The program outputs the VWAP calculations to a CSV file for further analysis or review.

- Raw ITCH 5.0 data downloaded from [here](https://emi.nasdaq.com/ITCH/Nasdaq%20ITCH/01302019.NASDAQ_ITCH50.gz)
- Format is defined by this [document](https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/NQ
TVITCHspecification.pdf)


## Dependencies

- C++20 or later
- POSIX compliant system (for memory-mapped files and threading support)

## Compile the Project

```
g++ -std=c++2a -o3 -o ItchVwapProcessor main.cpp -lpthread
```

## Run Program

After compiling the project, it can be ran via

```
./ItchVwapProcessor path/to/your/itchDatafile
```

If the itchDatafile path is not provided, the program will look for the file under the current folder.
## Result

- In file `output.csv`


## VWAP Assumptions

- `Cross Trade Messages` are **included** when calculating VWAP
- `Non-Printable Messages` are **Not included** when calculating VWAP

## Design Notes

- **MemoryMappedFileReader**: Handles the reading of large data files efficiently by mapping files into memory, reducing the overhead of I/O operations.
- **Producer-Consumer Model**: The architecture is built around the producer-consumer model, with:
  - a producer thread dedicated to reading data from memory-mapped files and consumer threads focused on processing this data
  - a consumer threads focused on processing this data and output vwap results
- **ThreadSafeQueue**: as a buffer and synchronization mechanism between producer and consumer
- **Message Parsing**: Implements a factory pattern to dynamically create message objects based on the ITCH message types

## Future Improvements

The current implementation relies on a single-threaded approach for both reading and processing data, adhering to the sequential order mandated by the ITCH5 data format. Future enhancements can introduce a multi-threaded architecture for both reading and processing the data.
