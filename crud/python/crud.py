import threading
import multiprocessing
import time
import os
import asyncio
import csv
import orjson


class Benchmark:
    value = 0
    lock = threading.Lock()
    last_element = None

    @classmethod
    def write_result(cls, test: str, iterations: int, duration: float):
        ops_per_sec = int(iterations / duration)
        # print(f"{test}: Elapsed time: {duration:.3f} seconds, OPS/sec: {ops_per_sec}")
        with open("benchmark.csv", "a", newline="") as csv_file:
            csv_writer = csv.writer(csv_file, delimiter='|')
            csv_writer.writerow(["python", test, ops_per_sec, cls.last_element])

    @classmethod
    def thread_mutex_loop(cls, iterations: int, mtx: threading.Lock):
        results = []
        with mtx:
            while Benchmark.value < 0:
                pass

        for i in range(iterations):
            with mtx:
                if i % 10 == 0:
                    Benchmark.value += 1
                    value_read = Benchmark.value
                else:
                    value_read = Benchmark.value
            results.append(value_read)


    @classmethod
    def thread_mutex(cls, iterations: int, over_subscribe: int = 1):
        mtx = threading.Lock()
        concurrency = os.cpu_count() * over_subscribe
        sub_iterations = max(1, iterations // concurrency)
        threads = []

        Benchmark.value = -1
        mtx.acquire()
        for i in range(concurrency):
            thread = threading.Thread(target=Benchmark.thread_mutex_loop, args=(sub_iterations, mtx))
            threads.append(thread)
            thread.start()

        # Start signal and timer
        Benchmark.value = 0
        start_time = time.perf_counter()
        mtx.release()

        for t in threads:
            t.join()

        end_time = time.perf_counter()
        cls.last_element = Benchmark.value
        Benchmark.write_result(f"Thread (mutex, oversub={over_subscribe})", iterations, end_time - start_time)

    @classmethod
    async def async_mutex_loop(cls, iterations: int, mtx: asyncio.Lock):
        results = []
        while Benchmark.value < 0:
            await asyncio.sleep(0)

        for i in range(iterations):
            async with mtx:
                if i % 10 == 0:
                    Benchmark.value += 1
                    value_read = Benchmark.value
                else:
                    value_read = Benchmark.value
            results.append(value_read)

    @classmethod
    async def async_mutex(cls, iterations: int, over_subscribe: int = 1):
        mtx = asyncio.Lock()
        concurrency = os.cpu_count() * over_subscribe
        sub_iterations = iterations // concurrency

        Benchmark.value = -1
        async with mtx:
            tasks = [asyncio.create_task(Benchmark.async_mutex_loop(sub_iterations, mtx)) for i in range(concurrency)]

        Benchmark.value = 0
        start_time = time.perf_counter()
        await asyncio.gather(*tasks)
        end_time = time.perf_counter()
        cls.last_element = Benchmark.value
        Benchmark.write_result(f"Async (mutex, oversub: {over_subscribe})", iterations, end_time - start_time)

    @classmethod
    def simple_int_loop(cls, iterations: int):
        results = []
        for i in range(iterations):
            if i % 10 == 0:
                Benchmark.value += 1
                value_read = Benchmark.value
            else:
                value_read = Benchmark.value
            results.append(value_read)
        cls.last_element = results[-1]


    @classmethod
    def simple_json_loop(cls, iterations: int):
        results = []
        for i in range(iterations):
            if i % 10 == 0:
                Benchmark.value += 1
                value_read = Benchmark.value
            else:
                value_read = Benchmark.value
            json = orjson.dumps({"value": value_read})
            results.append(json.decode("utf-8"))
        cls.last_element = results[-1]


    @classmethod
    def simple_int(cls, iterations: int):
        Benchmark.value = 0
        start_time = time.perf_counter()
        Benchmark.simple_int_loop(iterations)
        end_time = time.perf_counter()
        Benchmark.write_result("Single Thread INT", iterations, end_time - start_time)

    @classmethod
    def simple_json(cls, iterations: int):
        Benchmark.value = 0
        start_time = time.perf_counter()
        Benchmark.simple_json_loop(iterations)
        end_time = time.perf_counter()
        Benchmark.write_result("Single Thread JSON", iterations, end_time - start_time)

    @classmethod
    def process_loop(cls,
                     iterations: int,
                     lock: multiprocessing.Lock,
                     shared_memory: multiprocessing.Value):
        results = []
        # Wait for start signal
        while shared_memory.value < 0:
            pass

        for i in range(iterations):
            with lock:
                if i % 10 == 0:
                    shared_memory.value += 1
                    value_read = shared_memory.value
                else:
                    value_read = shared_memory.value

            results.append(value_read)
        # cls.last_element = results[-1]

    @classmethod
    def processes(cls, iterations: int, over_subscribe: int = 1):
        concurrency = os.cpu_count() * over_subscribe
        sub_iterations = iterations // concurrency

        lock = multiprocessing.Lock()
        value = multiprocessing.Value('i', -1)

        with lock:
            processes = []
            for i in range(concurrency):
                p = multiprocessing.Process(target=Benchmark.process_loop, args=(sub_iterations, lock, value))
                processes.append(p)
                p.start()

            # Start signal and timer
            value.value = 0
        start_time = time.perf_counter()

        for p in processes:
            p.join()

        end_time = time.perf_counter()
        cls.last_element = value.value
        Benchmark.write_result(f"Process (Lock, oversub={over_subscribe})", iterations, end_time - start_time)


def main():
    import sys

    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <iterations> <mode>")
        return 1

    iterations = int(sys.argv[1])
    mode = sys.argv[2]

    benchmark = Benchmark()
    if mode == "simple":
        benchmark.simple_int(iterations)
    elif mode == "json":
        benchmark.simple_json(iterations)
    elif mode == "process":
        for i in [1, 2, 4, 8, 16]:
            benchmark.processes(iterations, i)
    elif mode == "thread":
        for i in [1, 2, 4, 8, 16, 32, 64]:
            benchmark.thread_mutex(iterations, i)
    elif mode == "async":
        loop = asyncio.get_event_loop()
        for i in [1, 2, 4, 8, 16, 32, 64]:
            loop.run_until_complete(benchmark.async_mutex(iterations, i))
    elif mode == "all":
        benchmark.simple_int(iterations)
        benchmark.simple_json(iterations)
        benchmark.thread_mutex(iterations)
        loop = asyncio.get_event_loop()
        loop.run_until_complete(benchmark.async_mutex(iterations))
    else:
        print("Invalid mode.")
        return 1

    return 0


if __name__ == '__main__':
    main()
