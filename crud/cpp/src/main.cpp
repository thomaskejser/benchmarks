#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <mutex>
#include <thread>
#include <future>
#include <windows.h>
#include <sstream>
#include <string_view>
#include <nlohmann/json.hpp>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <atomic>
#include <algorithm>
#include <cstring>
#include "terje_itoa.hpp"

typedef int64_t data_t;

volatile data_t value;
std::mutex control;
std::vector<std::string> last_elements;

void write_result(std::string test, unsigned iterations, std::chrono::duration<double> duration) {

    std::ofstream csv_file("benchmark.csv", std::ios::app);
    auto secs = duration.count();
    auto ops_per_sec = int(iterations / secs);
    csv_file << "c++" << "|"
        << test << "|"
        << std::to_string(ops_per_sec) << "|"
        << last_elements.back() << "\n";
}

template<typename T>
void touch_data(unsigned process_number, std::vector<T>& result) {
    auto last_value = std::to_string(result.back());
    std::lock_guard<std::mutex> lock(control);
    last_elements.push_back(last_value);
}

template<>
void touch_data(unsigned process_number, std::vector<std::string>& result) {
    auto last_value = result.back();
    std::lock_guard<std::mutex> lock(control);
    last_elements.push_back(std::string(last_value));
}

template<>
void touch_data(unsigned process_number, std::vector<char*>& result) {
    auto last_value = std::string(result.back());
    std::lock_guard<std::mutex> lock(control);
    last_elements.push_back(last_value);
}


void thread_mutex_loop(unsigned thread_number, unsigned iterations, std::mutex& mtx) {
    std::vector<data_t> results;

    while (true) {
        /* Wait for the start signal */
        std::lock_guard<std::mutex> lock(mtx);
        if (value >= 0)
            break;
    }

    for (unsigned i = 0; i < iterations; ++i) {
        data_t value_read = 0;

        if (i % 10 == 0) {
            std::lock_guard<std::mutex> lock(mtx);
            value += 1;
            value_read = value;
        }
        else {
            std::lock_guard<std::mutex> lock(mtx);
            value_read = value;
        }
        results.push_back(value_read);
    }
    touch_data(thread_number, results);
}

void thread_mutex(unsigned iterations, unsigned over_subscribe = 1) {
    std::mutex mtx;
    unsigned concurrency = std::thread::hardware_concurrency() * over_subscribe;
    unsigned sub_iterations = iterations / concurrency;
    std::vector<std::thread> threads;

    value = -1;
    mtx.lock();
    for (unsigned i = 0; i < concurrency; ++i) {
        threads.emplace_back(thread_mutex_loop, i, sub_iterations, std::ref(mtx));
    }

    /* start signal and timer */
    value = 0;
    auto start_time = std::chrono::high_resolution_clock::now();
    mtx.unlock();

    for (auto& t : threads) {
        t.join();
    }
    auto end_time = std::chrono::high_resolution_clock::now();
    return write_result("Thread (mutex, oversub=" + std::to_string(over_subscribe)  + ")", iterations, end_time -
    start_time);
}


void async_mutex(unsigned iterations, unsigned over_subscribe = 1) {
    std::mutex mtx;
    unsigned concurrency = std::thread::hardware_concurrency() * over_subscribe ;
    unsigned sub_iterations = iterations / concurrency;

    std::vector<std::future<void>> futures;
    value = -1;
    mtx.lock();
    for (unsigned i = 0; i < concurrency; ++i) {
        futures.push_back(std::async(std::launch::async, thread_mutex_loop, i, sub_iterations, std::ref(mtx)));
    }

    /* start signal and timer */
    value = 0;
    auto start_time = std::chrono::high_resolution_clock::now();
    mtx.unlock();

    for (auto& future : futures) {
        future.get();
    }
    auto end_time = std::chrono::high_resolution_clock::now();
    return write_result("Async (mutex, oversub: " + std::to_string(over_subscribe) + ")" , iterations, end_time -
    start_time);
}


void simple_json_rapid_loop(unsigned iterations) {
    /* Optimise: We can allocate everything needed outside the hot loop */
    rapidjson::Document document;
    document.SetObject();
    rapidjson::Document::AllocatorType& allocator = document.GetAllocator();
    document.AddMember("value", rapidjson::Value(), allocator);
    rapidjson::StringBuffer buffer;

    std::vector<std::string> results;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    for (unsigned i = 0; i < iterations; ++i) {
        data_t value_read;
        if (i % 10 == 0) {
            value += 1;
            value_read = value;
        }
        else {
            value_read = value;
        }

        document["value"].SetInt64(value_read);

        buffer.Clear();
        document.Accept(writer);
        results.push_back(buffer.GetString());
    }
    touch_data(0, results);
}


void simple_json_lohmann_loop(unsigned iterations) {
    /* Make it simple, make it fast */

    std::vector<std::string> results;
    nlohmann::json j;

    for (unsigned i = 0; i < iterations; ++i) {
        data_t value_read = -1;
        if (i % 10 == 0) {
            value += 1;
            value_read = value;
        }
        else {
            value_read = value;
        }

        j["value"]= value_read;
        results.push_back(j.dump());
    }
    touch_data(0, results);
}

void simple_json_kejser_loop(unsigned iterations) {
    /* Make it simple, make it fast */

    std::vector<char*> results;

    /* Placeholder template we can blast itoa into and assume we have zero pad */
    char buffer[] = "{\"value\":0000000000}";

    for (unsigned i = 0; i < iterations; ++i) {
        data_t value_read = -1;
        if (i % 10 == 0) {
            value += 1;
            value_read = value;
        }
        else {
            value_read = value;
        }
        itoa(buffer+9, (uint32_t)value_read);
        results.push_back(buffer);
    }
    touch_data(0, results);
}


void simple_json_rapid(int iterations) {
    value = 0;
    auto start_time = std::chrono::high_resolution_clock::now();
    simple_json_rapid_loop(iterations);
    auto end_time = std::chrono::high_resolution_clock::now();
    return write_result("JSON (RapidJSON)", iterations, end_time - start_time);
}

void simple_json_lohmann(int iterations) {
    value = 0;
    auto start_time = std::chrono::high_resolution_clock::now();
    simple_json_lohmann(iterations);
    auto end_time = std::chrono::high_resolution_clock::now();
    return write_result("JSON (Lohmann)", iterations, end_time - start_time);
}

void simple_json(int iterations) {
    value = 0;
    auto start_time = std::chrono::high_resolution_clock::now();
    simple_json_rapid_loop(iterations);
    auto end_time = std::chrono::high_resolution_clock::now();
    return write_result("Single Thread JSON", iterations, end_time - start_time);
}


void simple_json_kejser(int iterations) {
    value = 0;
    auto start_time = std::chrono::high_resolution_clock::now();
    simple_json_kejser_loop(iterations);
    auto end_time = std::chrono::high_resolution_clock::now();
    return write_result("JSON (Kejser)", iterations, end_time - start_time);
}


void simple_int_loop(unsigned iterations) {
    std::vector<data_t> results;
    for (unsigned i = 0; i < iterations; ++i) {
        data_t value_read = -1;
        if (i % 10 == 0) {
            value += 1;
            value_read = value;
        }
        else {
            value_read = value;
        }
        results.push_back(value_read);
    }
    touch_data(0, results);

}

void simple_int(int iterations) {
    value = 0;
    auto start_time = std::chrono::high_resolution_clock::now();
    simple_int_loop(iterations);
    auto end_time = std::chrono::high_resolution_clock::now();
    return write_result("Single Thread INT", iterations, end_time - start_time);

}

struct SharedData {
    data_t shared_value;
};
const char* SHARED_MEMORY_NAME = TEXT("Global\\SharedMemory");
const char* SEMAPHORE_NAME = TEXT("Global\\SharedSemaphore");

void process_loop(unsigned process_number, unsigned iterations) {
    /* Now, grab pointers to all the stuff our spawner set up */
    HANDLE hMapFile = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, SHARED_MEMORY_NAME);
    if (hMapFile == NULL) {
        std::cerr << "Could not open file mapping object (" << GetLastError() << ")." << std::endl;
        return;
    }
    SharedData* hSharedData = (SharedData*) MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedData));
    if (hSharedData == NULL) {
        std::cerr << "Could not map view of file (" << GetLastError() << ")." << std::endl;
        CloseHandle(hMapFile);
        return;
    }
    HANDLE hSemaphore = OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, SEMAPHORE_NAME);
    if (hSemaphore == NULL) {
        std::cerr << "Could not open semaphore (" << GetLastError() << ")." << std::endl;
        UnmapViewOfFile(hSharedData);
        CloseHandle(hMapFile);
        return;
    }
    std::vector<data_t> result;
    /* Wait for the start signal */
    bool start = false;
    while (!start) {
        WaitForSingleObject(hSemaphore, INFINITE);
        start = hSharedData->shared_value >= 0;
        ReleaseSemaphore(hSemaphore, 1, NULL);
    }

    for (unsigned i = 0; i < iterations; ++i) {
        data_t value = -1;
        if (i % 10 == 0) {
            // We are writing
            WaitForSingleObject(hSemaphore, INFINITE);      // Lock
            hSharedData->shared_value++;
            ReleaseSemaphore(hSemaphore, 1, NULL);          // Unlock
        }
        else {
            // We are reading
            WaitForSingleObject(hSemaphore, INFINITE);      // Lock
            value = hSharedData->shared_value;
            ReleaseSemaphore(hSemaphore, 1, NULL);          // Unlock
        }

        result.push_back(value);
    }

    touch_data(process_number, result);
}

void processes(unsigned iterations, unsigned over_subscribe = 1) {
    /* The usual, disgusting incantation, to set up shared memory */
    HANDLE hMapFile = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(SharedData), SHARED_MEMORY_NAME);
    if (hMapFile == NULL) {
        std::cerr << "Could not create file mapping object (" << GetLastError() << ")." << std::endl;
        return;
    }
    SharedData* hSharedData = static_cast<SharedData*>(MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedData)));
    if (hSharedData == NULL) {
        std::cerr << "Failed to map view of file." << std::endl;
        CloseHandle(hMapFile);
        return;
    }
    hSharedData->shared_value = -1;
    HANDLE hSemaphore = CreateSemaphore(NULL, 1, 1, SEMAPHORE_NAME);
    if (hSemaphore == NULL) {
        std::cerr << "Failed to create semaphore." << std::endl;
        UnmapViewOfFile(hSharedData);
        CloseHandle(hMapFile);
        return;
    }
    /*
     * We now have:
     *
     * hSharedData: Some memory all processes can see that we can read/write to
     * hSemaphore: A locking structure to synchronise access
     *
     * Next up: We spawn processes. We don't want to account the time it takes to spawn them,
     * so we grab the semaphore and hold onto it while everyoen we start will wait for the "start signal"
     * This signal will be the seeting of the `shared_value` to 0
     * */

    WaitForSingleObject(hSemaphore, INFINITE);

    char executablePath[MAX_PATH];
    GetModuleFileName(NULL, executablePath, MAX_PATH);
    unsigned int core_count = std::thread::hardware_concurrency() * over_subscribe;

    int sub_iterations = iterations / core_count;

    std::vector<PROCESS_INFORMATION> processes;
    for (unsigned i = 0; i < core_count; ++i) {
        STARTUPINFO si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        std::string commandLine = std::string(executablePath)
                + " --child"
                + " " + std::to_string(i)
                + " " + std::to_string(sub_iterations);
        if (CreateProcess(NULL, &commandLine[0], NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            processes.push_back(pi);
        } else {
            std::cerr << "Failed to create child process (" << GetLastError() << ")." << std::endl;
        }
    }

    /* Tell everyone to start and start the timer */
    hSharedData->shared_value = 0;
    auto start_time = std::chrono::high_resolution_clock::now();
    ReleaseSemaphore(hSemaphore, 1, NULL);

    for (const auto& pi : processes) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    auto end_time = std::chrono::high_resolution_clock::now();
    write_result("Process (Semaphore, oversub=" + std::to_string(over_subscribe) + ")", iterations,  end_time -
    start_time);

    UnmapViewOfFile(hSharedData);
    CloseHandle(hMapFile);
    CloseHandle(hSemaphore);

}

void validate() {
    for (auto& e : last_elements) {
        std::cout << e << std::endl;
    }
}

int main(int argc, char* argv[]) {
    if (argc > 1 && std::string(argv[1]) == "--child") {
        // We were spawned by the process runner
        int process_number = std::stoi(argv[2]);
        int iterations = std::stoi(argv[3]);
        process_loop(process_number, iterations);
        return 0;
    }

    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <iterations> <mode>" << std::endl;
        std::cerr << "Mode: json, int, thread, async, or process" << std::endl;
        return 1;
    }

    int iterations = std::stoi(argv[1]);
    std::string mode = argv[2];

    if (mode == "process") {
        for (unsigned i = 1; i <= 64; i*=2) {
            processes(iterations, i);
        }
    }
    else if (mode == "thread") {
        for (unsigned i = 1; i <= 64; i*=2) {
            thread_mutex(iterations, i);
        }
    }
    else if (mode == "simple") {
        simple_int(iterations);
    }
    else if (mode == "json") {
        simple_json(iterations);
        simple_json_lohmann(iterations);
        simple_json_rapid(iterations);
        simple_json_kejser(iterations);
    }
    else if (mode == "async") {
        for (unsigned i = 1; i <= 64; i*=2) {
            async_mutex(iterations, i);
        }
    }
    else if (mode == "all") {
        processes(iterations);
        thread_mutex(iterations);
        simple_int(iterations);
        simple_json_rapid(iterations);
        async_mutex(iterations);
    }
    else {
        std::cerr << "Invalid mode." << std::endl;
        return 1;
    }

    // validate();
    return 0;
}


