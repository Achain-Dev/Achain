#include <thread>
#include <atomic>
#include <queue>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <future>
#include <vector>


class function_wrapper
{
    struct impl_base{
        virtual void call() = 0;
        virtual ~impl_base() {}
    };
    std::unique_ptr<impl_base> impl;
    template<typename F>
    struct impl_type : impl_base
    {
        F f;
        impl_type(F&& f_) : f(std::move(f_)){}
        void call(){ f(); }
    };
public:
    template<typename F>
    function_wrapper(F&& f) :
        impl(new impl_type<F>(std::move(f)))
    {}

    void operator()(){ impl->call(); }

    function_wrapper() = default;

    function_wrapper(function_wrapper&& other) :
        impl(std::move(other.impl))
    {}

    function_wrapper& operator=(function_wrapper&& other)
    {
        impl = std::move(other.impl);
        return *this;
    }

    function_wrapper(const function_wrapper&) = delete;

    function_wrapper(function_wrapper&) = delete;

    function_wrapper& operator=(function_wrapper&) = delete;

};


template<class T>
class thread_safe_queue
{
private:
    mutable std::mutex mut;
    std::queue<std::shared_ptr<T>> data_queue;
    std::condition_variable data_con;
public:
    thread_safe_queue(){}
    thread_safe_queue(thread_safe_queue const& other)
    {
        std::lock_guard<std::mutex> lk(other.mut);
        data_queue = other.data_queue;
    }
    void push(T tValue)
    {
        std::shared_ptr<T> data(std::make_shared<T>(std::move(tValue)));
        std::lock_guard<std::mutex> lk(mut);
        data_queue.push(data);
        data_con.notify_one();
    }
    void wait_and_pop(T& tValue)
    {
        std::unique_lock<std::mutex> lk(mut);
        data_con.wait(lk, [this]{return !data_queue.empty(); });
        tValue = std::move(*data_queue.front());
        data_queue.pop();
    }
    std::shared_ptr<T>wait_and_pop()
    {
        std::unique_lock<std::mutex> lk(mut);
        data_con.wait(lk, [this]{return !data_queue.empty(); });
        std::shared_ptr<T> ret(std::make_shared<T>(data_queue.front()));
        data_queue.pop();
        return ret;
    }
    bool try_pop(T& tValue)
    {
        std::lock_guard<std::mutex> lk(mut);
        if (data_queue.empty())
            return false;
        tValue = std::move(*data_queue.front());
        data_queue.pop();
        return true;
    }

    std::shared_ptr<T> try_pop()
    {
        std::lock_guard<std::mutex> lk(mut);
        if (data_queue.empty())
            return std::shared_ptr<T>();
        std::shared_ptr<T> ret(std::make_shared(data_queue.front()));
        data_queue.pop();
        return ret;
    }

    bool empty() const
    {
        std::lock_guard<std::mutex> lk(mut);
        return data_queue.empty();
    }
};


class join_threads
{
    std::vector<std::thread>& threads;
public:
    explicit join_threads(std::vector<std::thread>& threads_) :
        threads(threads_)
    {}
    ~join_threads()
    {
        for (unsigned long i = 0; i < threads.size(); ++i)
        {
            if (threads[i].joinable())
                threads[i].join();
        }
    }

};


class ThreadPool
{
    std::atomic_bool done;
    std::vector<std::thread> threads;
    join_threads joiner;
    thread_safe_queue<function_wrapper> work_queue;
    void worker_thread()
    {
        while (!done)
        {
            function_wrapper task;
            work_queue.wait_and_pop(task);
            task();

//             else
//             {
//                 std::this_thread::yield();
//             }

        }
    }
public:
#ifndef WIN32
    ThreadPool() :done(false), joiner(threads)
#else
    ThreadPool():done(std::atomic<bool>(false)),joiner(threads)
#endif
    {
        unsigned const thread_count = std::thread::hardware_concurrency() - 1;

        try{
            for (unsigned i = 0; i < thread_count; ++i)
            {
                threads.push_back(
                    std::thread(&ThreadPool::worker_thread, this));
            }
        }
        catch (...)
        {
            done = true;
            throw;
        }
    }
    ~ThreadPool()
    {
        
        done = true;
        for (int i = 0; i < threads.size(); ++i)
        {
            auto fut = submit([=]()->bool{return true; });
            fut.get();
        }
        
    }
    template<typename FunctionType>
    std::future<typename std::result_of<FunctionType()>::type> submit(FunctionType f)
    {
        typedef typename std::result_of<FunctionType()>::type result_type;
        std::packaged_task<result_type()> task(std::move(f));
        std::future<result_type> res(task.get_future());
        work_queue.push(std::move(task));
        return res;
    }


};