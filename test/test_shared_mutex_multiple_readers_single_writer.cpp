// (C) Copyright 2006-7 Anthony Williams
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_THREAD_VERSION 2
#define BOOST_THREAD_PROVIDES_INTERRUPTIONS
#define BOOST_TEST_MODULE Boost.Threads: shared_mutex test suite

#include <boost/test/unit_test.hpp>
#include <boost/chrono.hpp>
#include <boost/thread/thread.hpp>
#include "./util.inl"
#include <iostream>

#define CHECK_LOCKED_VALUE_EQUAL(mutex_name,value,expected_value)    \
    {                                                                \
        boost::unique_lock<boost::mutex> lock(mutex_name);                  \
        BOOST_CHECK_EQUAL(value,expected_value);                     \
    }

// Demonstrate unexpected behaviour caused by pthread/shared_mutex.hpp:259

static const boost::chrono::seconds reader_run_time(2);
static const boost::chrono::seconds writer_delay(1);

class reader_thread
{
    boost::shared_mutex& rwm;
    boost::chrono::steady_clock::time_point start_time;

public:
    reader_thread(boost::shared_mutex &rwm_,
                  boost::chrono::steady_clock::time_point start_time_)
        : rwm(rwm_), start_time(start_time_) {}

    void operator()()
    {
        // Repeatedly do a shared unlock + lock
        rwm.lock_shared();
        while(boost::chrono::steady_clock::now() - start_time <= reader_run_time)
        {
            boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
            rwm.unlock_shared();
            rwm.lock_shared();
        }
        rwm.unlock_shared();
    }
};

class writer_thread
{
    boost::shared_mutex& rwm;
    boost::chrono::steady_clock::time_point start_time;
    boost::chrono::steady_clock::duration& lock_duration;

public:
    writer_thread(boost::shared_mutex &rwm_,
                  boost::chrono::steady_clock::time_point start_time_,
                  boost::chrono::steady_clock::duration& lock_duration_)
        : rwm(rwm_), start_time(start_time_), lock_duration(lock_duration_) {}

    void operator()()
    {
        boost::this_thread::sleep_for(writer_delay);
        rwm.lock();
        boost::chrono::steady_clock::time_point locked_time = boost::chrono::steady_clock::now();
        rwm.unlock();

        lock_duration = locked_time - start_time;
    }
};

BOOST_AUTO_TEST_CASE(test_multiple_readers_single_writer)
{
  std::cout << __LINE__ << std::endl;
    unsigned const number_of_threads = boost::thread::hardware_concurrency();

    boost::thread_group pool;

    boost::shared_mutex rw_mutex;
    boost::chrono::steady_clock::duration lock_duration;

    try
    {
        boost::chrono::steady_clock::time_point start_time = boost::chrono::steady_clock::now();

        for(unsigned i=0;i<number_of_threads;++i)
        {
            pool.create_thread(reader_thread(rw_mutex, start_time));
        }
        pool.create_thread(writer_thread(rw_mutex, start_time, lock_duration));

        pool.join_all();

        BOOST_CHECK_GE(lock_duration, writer_delay);
        // Check that exclusive lock was gained while 'reader' threads were working
        BOOST_CHECK_LT(lock_duration, reader_run_time);
    }
    catch(...)
    {
        pool.interrupt_all();
        pool.join_all();
        throw;
    }
}
