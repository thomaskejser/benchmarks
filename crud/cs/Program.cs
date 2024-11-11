using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using System.Linq;
using System.Text;
using Jil;
using Utf8Json;
using Utf8Json.Resolvers;

class Benchmark
{
    private static int Value = 0;
    private static readonly object Lock = new object();
    private static string LastElement;

    public static void WriteResult(string test, int iterations, double duration)
    {
        int opsPerSec = (int)(iterations / duration);
        using (var writer = new StreamWriter("benchmark.csv", true))
        {
            writer.WriteLine($"c#|{test}|{opsPerSec}|{LastElement}");
        }
    }

    public static void ThreadMutexLoop(int iterations, object mtx)
    {
        List<int> results = new List<int>();
        lock (mtx)
        {
            while (Value < 0)
            {
                // Wait
            }
        }

        for (int i = 0; i < iterations; i++)
        {
            lock (mtx)
            {
                if (i % 10 == 0)
                {
                    Value++;
                }

                results.Add(Value);
            }
        }
    }

    public static void ThreadMutex(int iterations, int overSubscribe = 1)
    {
        object mtx = new object();
        int concurrency = Environment.ProcessorCount * overSubscribe;
        int subIterations = Math.Max(1, iterations / concurrency);
        List<Thread> threads = new List<Thread>();

        Value = -1;
        lock (mtx)
        {
            for (int i = 0; i < concurrency; i++)
            {
                Thread thread = new Thread(() => ThreadMutexLoop(subIterations, mtx));
                threads.Add(thread);
                thread.Start();
            }

            // Start signal and timer
            Value = 0;
        }

        var startTime = Stopwatch.GetTimestamp();
        foreach (var thread in threads)
        {
            thread.Join();
        }

        var endTime = Stopwatch.GetTimestamp();
        LastElement = Value.ToString();
        WriteResult($"Thread (mutex, oversub={overSubscribe})", iterations,
            (endTime - startTime) / (double)Stopwatch.Frequency);
        
    }

    public static async Task AsyncMutexLoop(int iterations, SemaphoreSlim mtx)
    {
        List<int> results = new List<int>();
        while (Value < 0)
        {
            await Task.Delay(1);
        }

        for (int i = 0; i < iterations; i++)
        {
            await mtx.WaitAsync();
            try
            {
                if (i % 10 == 0)
                {
                    Value++;
                }

                results.Add(Value);
            }
            finally
            {
                mtx.Release();
            }
        }
    }

    public static async Task AsyncMutex(int iterations, int overSubscribe = 1)
    {
        SemaphoreSlim mtx = new SemaphoreSlim(1, 1);
        int concurrency = Environment.ProcessorCount * overSubscribe;
        int subIterations = iterations / concurrency;

        
        Value = -1;
        var tasks = new List<Task>();
        for (int i = 0; i < concurrency; i++)
        {
            tasks.Add(AsyncMutexLoop(subIterations, mtx));
        }

        Value = 0;
        var startTime = Stopwatch.GetTimestamp();
        await Task.WhenAll(tasks);
        var endTime = Stopwatch.GetTimestamp();
        LastElement = Value.ToString();
        WriteResult($"Async (mutex, oversub={overSubscribe})", iterations,
            (endTime - startTime) / (double)Stopwatch.Frequency);
    }

    public static void SimpleJsonLoopNaive(int iterations)
    {
        JsonThing o = new JsonThing();
        List<string> results = new List<string>();
        for (var i = 0; i < iterations; i++)
        {
            if (i % 10 == 0)
            {
                Value++;
            }

            (o.Name, o.Value) = ("name", Value);
            // NOTE: stack allocation would be an option here if Newtonsoft supported it
            var json = JSON.Serialize(o);
            results.Add(json);
        }

        LastElement = results.Last();
    }

    public class JsonThing
    {
        public string Name;
        public int Value;
    }
    public static void SimpleJsonLoop(int iterations)
    {
        JsonThing o = new JsonThing();
        JsonSerializer.SetDefaultResolver(StandardResolver.AllowPrivateExcludeNullSnakeCase);
        const string name = "name"; 
        using (var writer = new MemoryStream())
        {
            for (var i = 0; i < iterations; i++)
            {
                if (i % 10 == 0)
                {
                    Value++;
                }

                (o.Name, o.Value) = (name, Value);
                JsonSerializer.ToJsonString(o);
            }
        }

        // LastElement = builderLength.ToString();
    }

    public static void SimpleJson(int iterations)
    {
        Value = 0;
        var startTime = Stopwatch.GetTimestamp();
        SimpleJsonLoop(iterations);
        var endTime = Stopwatch.GetTimestamp();
        WriteResult("Single Thread JSON", iterations, (endTime - startTime) / (double)Stopwatch.Frequency);
    }

    public static void SimpleIntLoop(int iterations)
    {
        List<int> results = new List<int>();
        for (var i = 0; i < iterations; i++)
        {
            if (i % 10 == 0)
            {
                Value++;
            }

            results.Add(Value);
        }

        LastElement = results.Last().ToString();
    }

    public static void SimpleInt(int iterations)
    {
        Value = 0;
        var startTime = Stopwatch.GetTimestamp();
        SimpleIntLoop(iterations);
        var endTime = Stopwatch.GetTimestamp();
        WriteResult("Single Thread INT", iterations, (endTime - startTime) / (double)Stopwatch.Frequency);
    }

    static void Main(string[] args)
    {
        if (args.Length < 2)
        {
            Console.WriteLine($"Usage: {AppDomain.CurrentDomain.FriendlyName} <iterations> <mode>");
            return;
        }

        var iterations = int.Parse(args[0]);
        var mode = args[1];

        switch (mode)
        {
            case "simple":
                SimpleInt(iterations);
                break;
            case "json":
                SimpleJson(iterations);
                break;
            case "thread":
                for (var i = 1; i <= 64; i *= 2)
                {
                    ThreadMutex(iterations, i);
                }

                break;
            case "async":
                for (var i = 1; i <= 64; i *= 2)
                {
                    AsyncMutex(iterations, i).Wait();
                }
                break;
            default:
                Console.WriteLine("Invalid mode.");
                break;
        }
    }
}