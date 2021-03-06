// Copyright (C) 2015-2016 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <thread>
#include <mutex>
#include <condition_variable>

#include <gtest/gtest.h>

#include <vsomeip/vsomeip.hpp>

#include "someip_test_globals.hpp"

using namespace vsomeip;

class someip_application_test: public ::testing::Test {
public:
    someip_application_test() :
            registered_(false) {

    }
protected:
    void SetUp() {
        app_ = runtime::get()->create_application("application_test");
        if (!app_->init()) {
            VSOMEIP_ERROR << "Couldn't initialize application";
            EXPECT_TRUE(false);
        }

        app_->register_state_handler(
                std::bind(&someip_application_test::on_state, this,
                        std::placeholders::_1));
    }

    void on_state(vsomeip::state_type_e _state) {
        registered_ = (_state == vsomeip::state_type_e::ST_REGISTERED);
    }

    bool registered_;
    std::shared_ptr<application> app_;
};

/**
 * @test Start and stop application
 */
TEST_F(someip_application_test, start_stop_application)
{
    std::thread t([&](){
        app_->start();
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    app_->stop();
    t.join();
}

/**
 * @test Start and stop application multiple times
 */
TEST_F(someip_application_test, start_stop_application_multiple)
{
    for (int i = 0; i < 10; ++i) {
        std::thread t([&]() {
            app_->start();
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        app_->stop();
        t.join();
    }
}

/**
 * @test Start and stop application multiple times and offer a service
 */
TEST_F(someip_application_test, start_stop_application_multiple_offer_service)
{
    for (int i = 0; i < 10; ++i) {
        std::thread t([&]() {
            app_->start();
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        app_->offer_service(vsomeip_test::TEST_SERVICE_SERVICE_ID, vsomeip_test::TEST_SERVICE_INSTANCE_ID);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        app_->stop_offer_service(vsomeip_test::TEST_SERVICE_SERVICE_ID, vsomeip_test::TEST_SERVICE_INSTANCE_ID);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        app_->stop();
        t.join();
    }
}

/**
 * @test Try to start an already running application again
 */
TEST_F(someip_application_test, restart_without_stopping)
{
    std::thread t([&]() {
        app_->start();

    });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    VSOMEIP_WARNING << "An error message should appear now";
    // should print error
    app_->start();
    app_->stop();
    t.join();
}

/**
 * @test Try to stop a running application twice
 */
TEST_F(someip_application_test, stop_application_twice)
{
    std::thread t([&]() {
        app_->start();

    });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    app_->stop();
    t.join();
    app_->stop();
}

class someip_application_shutdown_test: public ::testing::Test {

protected:
    void SetUp() {
        is_registered_ = false;
        is_available_ = false;

        app_ = runtime::get()->create_application("application_test");
        if (!app_->init()) {
            VSOMEIP_ERROR << "Couldn't initialize application";
            EXPECT_TRUE(false);
        }

        app_->register_message_handler(vsomeip_test::TEST_SERVICE_SERVICE_ID,
                vsomeip_test::TEST_SERVICE_INSTANCE_ID,
                vsomeip_test::TEST_SERVICE_METHOD_ID_SHUTDOWN,
                std::bind(&someip_application_shutdown_test::on_message_shutdown, this,
                        std::placeholders::_1));

        app_->register_state_handler(
                std::bind(&someip_application_shutdown_test::on_state, this,
                        std::placeholders::_1));
        app_->register_availability_handler(
                vsomeip_test::TEST_SERVICE_SERVICE_ID,
                vsomeip_test::TEST_SERVICE_INSTANCE_ID,
                std::bind(&someip_application_shutdown_test::on_availability,
                        this, std::placeholders::_1, std::placeholders::_2,
                        std::placeholders::_3));

        shutdown_thread_ = std::thread(&someip_application_shutdown_test::send_shutdown_message, this);

        app_->start();
    }

    void TearDown() {
        shutdown_thread_.join();
        app_->stop();
    }

    void on_state(vsomeip::state_type_e _state) {
        if(_state == vsomeip::state_type_e::ST_REGISTERED)
        {
            std::lock_guard<std::mutex> its_lock(mutex_);
            is_registered_ = true;
            cv_.notify_one();
        }
    }

    void on_availability(vsomeip::service_t _service,
                         vsomeip::instance_t _instance, bool _is_available) {
        (void)_service;
        (void)_instance;
        if(_is_available) {
            std::lock_guard<std::mutex> its_lock(mutex_);
            is_available_ = _is_available;
            cv_.notify_one();
        }
    }

    void on_message_shutdown(const std::shared_ptr<message>& _request)
    {
        (void)_request;
        VSOMEIP_INFO << "Shutdown method was called, going down now.";
        app_->clear_all_handler();
        app_->stop();
    }

    void send_shutdown_message() {
        std::unique_lock<std::mutex> its_lock(mutex_);
        while(!is_registered_) {
            cv_.wait(its_lock);
        }
        app_->offer_service(vsomeip_test::TEST_SERVICE_SERVICE_ID, vsomeip_test::TEST_SERVICE_INSTANCE_ID);
        while(!is_available_) {
            cv_.wait(its_lock);
        }

        std::shared_ptr<message> r = runtime::get()->create_request();
        r->set_service(vsomeip_test::TEST_SERVICE_SERVICE_ID);
        r->set_instance(vsomeip_test::TEST_SERVICE_INSTANCE_ID);
        r->set_method(vsomeip_test::TEST_SERVICE_METHOD_ID_SHUTDOWN);
        app_->send(r);
    }

    bool is_registered_;
    bool is_available_;
    std::shared_ptr<application> app_;
    std::condition_variable cv_;
    std::mutex mutex_;
    std::thread shutdown_thread_;
};

/**
 * @test Stop the application through a method invoked from a dispatcher thread
 */
TEST_F(someip_application_shutdown_test, stop_application_from_dispatcher_thread) {

}

#ifndef WIN32
int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
#endif




