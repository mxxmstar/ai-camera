// ASIO Demo - Asynchronous Timer Example
//
// This demo demonstrates the basic usage of ASIO (standalone mode):
//   1. Create an io_context (the core I/O event loop)
//   2. Use a steady_timer for asynchronous waiting
//   3. Use post() to dispatch work across threads
//   4. Run the event loop with multiple threads

#include <asio.hpp>
#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

// -------------------------------------------------------------------
// 1. Basic asynchronous timer callback
// -------------------------------------------------------------------
void on_timer_expiry(const asio::error_code &ec)
{
    if (!ec)
    {
        std::cout << "[Timer] Hello from ASIO! Timer expired." << std::endl;
    }
    else
    {
        std::cerr << "[Timer] Error: " << ec.message() << std::endl;
    }
}

// -------------------------------------------------------------------
// 2. Demo 1: Simple single-threaded timer
// -------------------------------------------------------------------
void demo_simple_timer()
{
    std::cout << "\n========== Demo 1: Simple Timer (Single Thread) ==========" << std::endl;

    asio::io_context io_context;

    // Create a timer that expires after 1 second
    asio::steady_timer timer(io_context, std::chrono::seconds(1));

    // Set up an asynchronous wait
    timer.async_wait(&on_timer_expiry);

    std::cout << "[Main] Starting io_context run (waiting for timer)..." << std::endl;

    // Run the event loop – blocks until all work is done
    io_context.run();

    std::cout << "[Main] io_context.run() finished." << std::endl;
}

// -------------------------------------------------------------------
// 3. Demo 2: Repeated timer (fires every 500ms, 5 times)
// -------------------------------------------------------------------
class RepeatingTimer : public std::enable_shared_from_this<RepeatingTimer>
{
public:
    explicit RepeatingTimer(asio::io_context &io_context)
        : timer_(io_context), count_(0)
    {
    }

    void start()
    {
        // Schedule the first tick
        tick();
    }

private:
    void tick()
    {
        if (count_ >= 5)
        {
            std::cout << "[RepeatingTimer] Done. Fired " << count_ << " times." << std::endl;
            return;
        }

        timer_.expires_after(std::chrono::milliseconds(500));
        timer_.async_wait(
            [self = shared_from_this()](const asio::error_code &ec)
            {
                if (!ec)
                {
                    ++self->count_;
                    std::cout << "[RepeatingTimer] Tick #" << self->count_ << std::endl;
                    self->tick();
                }
            });
    }

    asio::steady_timer timer_;
    int count_;
};

void demo_repeating_timer()
{
    std::cout << "\n========== Demo 2: Repeating Timer ==========" << std::endl;

    asio::io_context io_context;

    auto repeater = std::make_shared<RepeatingTimer>(io_context);
    repeater->start();

    io_context.run();
}

// -------------------------------------------------------------------
// 4. Demo 3: Multi-threaded io_context with post()
// -------------------------------------------------------------------
void demo_multithread()
{
    std::cout << "\n========== Demo 3: Multi-threaded io_context ==========" << std::endl;

    asio::io_context io_context;

    // Post several tasks to the io_context
    for (int i = 0; i < 4; ++i)
    {
        asio::post(
            io_context,
            [i]
            {
                std::cout << "[Worker " << i << "] Running on thread "
                          << std::this_thread::get_id() << std::endl;
            });
    }

    // Post a timer task inline
    auto timer = std::make_shared<asio::steady_timer>(io_context, std::chrono::milliseconds(100));
    timer->async_wait(
        [timer](const asio::error_code &ec)
        {
            if (!ec)
            {
                std::cout << "[Timer in thread] Expired on thread "
                          << std::this_thread::get_id() << std::endl;
            }
        });

    // Run the io_context with 3 worker threads
    std::cout << "[Main] Running io_context with 3 threads..." << std::endl;

    std::vector<std::thread> threads;
    for (int i = 0; i < 3; ++i)
    {
        threads.emplace_back(
            [&io_context]
            {
                io_context.run();
            });
    }

    // Wait for all threads to finish
    for (auto &t : threads)
    {
        t.join();
    }

    std::cout << "[Main] All threads done." << std::endl;
}

// -------------------------------------------------------------------
// 5. Demo 4: TCP daytime client (demonstrates networking)
// -------------------------------------------------------------------
void demo_tcp_daytime_client()
{
    std::cout << "\n========== Demo 4: TCP Daytime Client ==========" << std::endl;

    asio::io_context io_context;

    // Resolve the host name
    asio::ip::tcp::resolver resolver(io_context);
    asio::ip::tcp::socket socket(io_context);

    asio::error_code ec;

    // Try to connect to a daytime server (time.nist.gov or a local one)
    auto endpoints = resolver.resolve("time.nist.gov", "daytime", ec);
    if (ec)
    {
        std::cout << "[TCP] Resolve failed (expected if no network): "
                  << ec.message() << std::endl;
        std::cout << "[TCP] Skipping network demo." << std::endl;
        return;
    }

    asio::connect(socket, endpoints, ec);
    if (ec)
    {
        std::cout << "[TCP] Connect failed: " << ec.message() << std::endl;
        return;
    }

    // Read the response
    asio::streambuf buffer;
    asio::read_until(socket, buffer, '\n', ec);

    if (!ec)
    {
        std::string line;
        std::istream is(&buffer);
        std::getline(is, line);
        std::cout << "[TCP] Daytime: " << line << std::endl;
    }
    else
    {
        std::cout << "[TCP] Read failed: " << ec.message() << std::endl;
    }
}

// -------------------------------------------------------------------
// Entry point
// -------------------------------------------------------------------
int main()
{
    std::cout << "============================================" << std::endl;
    std::cout << "  ASIO " << ASIO_VERSION / 100000 << "."
              << (ASIO_VERSION / 100) % 1000 << "."
              << ASIO_VERSION % 100 << " Demo" << std::endl;
    std::cout << "  Compiler: " << __cplusplus << " (C++ standard)" << std::endl;
    std::cout << "  Standalone mode: " << ASIO_STANDALONE << std::endl;
    std::cout << "============================================" << std::endl;

    demo_simple_timer();
    demo_repeating_timer();
    demo_multithread();
    demo_tcp_daytime_client();

    std::cout << "\nAll demos completed successfully!" << std::endl;
    return 0;
}