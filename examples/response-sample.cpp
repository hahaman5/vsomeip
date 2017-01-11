// Copyright (C) 2014-2016 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#ifndef VSOMEIP_ENABLE_SIGNAL_HANDLING
#include <csignal>
#endif
#include <chrono>
#include <condition_variable>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>
#include <string>
#include <ctime>
#include <chrono>

#include <vsomeip/vsomeip.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/tokenizer.hpp>

#include "sample-ids.hpp"

class service_sample {
public:
    service_sample(bool _use_static_routing) :
            app_(vsomeip::runtime::get()->create_application()),
            request_(vsomeip::runtime::get()->create_request(false)),
            is_registered_(false),
            use_static_routing_(_use_static_routing),
            blocked_(false),
            running_(true),
            is_available_(false),
            steer_angle(0),
            offer_thread_(std::bind(&service_sample::run, this)),
            thr_sender_(std::bind(&service_sample::sender, this)),
            dev_mgr_thread_(std::bind(&service_sample::run_map_mgr, this)) {
    }

    void init() {
        std::lock_guard<std::mutex> its_lock(mutex_);

        app_->init();
        app_->register_state_handler(
                std::bind(&service_sample::on_state, this,
                        std::placeholders::_1));
        app_->register_message_handler(
                SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID, SAMPLE_METHOD_ID,
                std::bind(&service_sample::on_message, this,
                        std::placeholders::_1));


        std::shared_ptr< vsomeip::payload > its_payload = vsomeip::runtime::get()->create_payload();
        std::cout << "Static routing " << (use_static_routing_ ? "ON" : "OFF")
                  << std::endl;

        /* steering request init */
        /*
        app_->register_message_handler(
                CONTROL_SERVICE_ID, CONTROL_INSTANCE_ID, CONTROL_METHOD_ID,
                std::bind(&service_sample::on_message_resp, this,
                        std::placeholders::_1));
                        */
        request_->set_service(SAMPLE_SERVICE_ID);
        request_->set_instance(SAMPLE_INSTANCE_ID);
        request_->set_method(SAMPLE_METHOD_ID);

        app_->register_availability_handler(CONTROL_SERVICE_ID, CONTROL_INSTANCE_ID,
                std::bind(&service_sample::on_availability,
                          this,
                          std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    }

    void start() {
        app_->start();
    }

#ifndef VSOMEIP_ENABLE_SIGNAL_HANDLING
    /*
     * Handle signal to shutdown
     */
    void stop() {
        running_ = false;
        blocked_ = true;
        condition_.notify_one();
        offer_thread_.join();
        app_->stop();
    }
#endif

    void offer() {
        app_->offer_service(SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID);
        app_->offer_service(SAMPLE_SERVICE_ID + 1, SAMPLE_INSTANCE_ID);
    }

    void stop_offer() {
        app_->stop_offer_service(SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID);
        app_->stop_offer_service(SAMPLE_SERVICE_ID + 1, SAMPLE_INSTANCE_ID);
    }

    void on_state(vsomeip::state_type_e _state) {
        std::cout << "Application " << app_->get_name() << " is "
                << (_state == vsomeip::state_type_e::ST_REGISTERED ?
                        "registered." : "deregistered.")
                << std::endl;

        if (_state == vsomeip::state_type_e::ST_REGISTERED) {
            if (!is_registered_) {
                is_registered_ = true;
                blocked_ = true;
                condition_.notify_one();
            }
        } else {
            is_registered_ = false;
        }
    }

    void on_message(const std::shared_ptr<vsomeip::message> &_request) {
        std::cout << "Received a message with Client/Session [" << std::setw(4)
            << std::setfill('0') << std::hex << _request->get_client() << "/"
            << std::setw(4) << std::setfill('0') << std::hex
            << _request->get_session() << "]"
            << std::endl;

		std::shared_ptr< vsomeip::payload > its_payload = _request->get_payload();
		vsomeip::byte_t *arr= its_payload->get_data();
        uint32_t len = its_payload->get_length();
        std::string str((char *)arr,len);
		
        std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
        now += std::chrono::seconds(5);
        dev_map[str] = now;
        send_dev_map();

/*
        std::shared_ptr<vsomeip::message> its_response
            = vsomeip::runtime::get()->create_response(_request);

        std::shared_ptr<vsomeip::payload> its_payload
            = vsomeip::runtime::get()->create_payload();
        std::vector<vsomeip::byte_t> its_payload_data;
        for (std::size_t i = 0; i < 120; ++i)
        its_payload_data.push_back(i % 256);
        its_payload->set_data(its_payload_data);
        its_response->set_payload(its_payload);

        app_->send(its_response, true);
*/
    }

    void run() {
        std::unique_lock<std::mutex> its_lock(mutex_);
        while (!blocked_)
            condition_.wait(its_lock);

        bool is_offer(true);

        if (use_static_routing_) {
            offer();
            while (running_);
        } else {
            while (running_) {
                if (is_offer)
                    offer();
                else
                    stop_offer();

                for (int i = 0; i < 10 && running_; i++)
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                is_offer = !is_offer;
            }
        }
    }

    void run_map_mgr() {
        while(1){
            std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
            bool is_mod = false;
            for(auto it = dev_map.begin(); it != dev_map.end();) {
                if(it->second <= now){
                    it = dev_map.erase(it);
                    std::cout << it->first << " is timed out" << std::endl;
                    is_mod = true;
                }else{
                    it++;
                }
            }

            if(is_mod){
                send_dev_map();
                is_mod = false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }

    }

    void send_dev_map() {
        /* TODO: write some code to send dev_map to head unit app */
    }

    void on_availability(vsomeip::service_t _service, vsomeip::instance_t _instance, bool _is_available) {
        std::cout << "Service ["
                << std::setw(4) << std::setfill('0') << std::hex << _service << "." << _instance
                << "] is "
                << (_is_available ? "available." : "NOT available.")
                << std::endl;

        if (CONTROL_SERVICE_ID == _service && CONTROL_INSTANCE_ID == _instance) {
            if (is_available_  && !_is_available) {
                is_available_ = false;
            } else if (_is_available && !is_available_) {
                is_available_ = true;
            }
        }
    }

    void sender() {

        int old_val = 0;
        while(true){
            /* TODO: get steering angle from head unit app */

            if(is_available_ && old_val != steer_angle){
                std::shared_ptr< vsomeip::payload > its_payload = vsomeip::runtime::get()->create_payload();
                unsigned char bytes[sizeof steer_angle];
                std::copy(static_cast<const char*>(static_cast<const void*>(&steer_angle)),
                        static_cast<const char*>(static_cast<const void*>(&steer_angle)) + sizeof steer_angle,
                        bytes);

                std::vector< vsomeip::byte_t > its_payload_data(std::begin(bytes),std::end(bytes));
                request_->set_payload(its_payload);
                app_->send(request_, true);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

private:
    std::shared_ptr<vsomeip::application> app_;
    bool is_registered_;
    bool use_static_routing_;

    std::mutex mutex_;
    std::condition_variable condition_;
    bool blocked_;
    bool running_;

    // blocked_ must be initialized before the thread is started.
    std::thread offer_thread_;
    std::thread dev_mgr_thread_;
    std::map<std::string, std::chrono::system_clock::time_point> dev_map;

    // members for steering request
    std::shared_ptr< vsomeip::message > request_;
    bool is_available_;
    std::thread thr_sender_;
    int steer_angle;
};

#ifndef VSOMEIP_ENABLE_SIGNAL_HANDLING
    service_sample *its_sample_ptr(nullptr);
    void handle_signal(int _signal) {
        if (its_sample_ptr != nullptr &&
                (_signal == SIGINT || _signal == SIGTERM))
            its_sample_ptr->stop();
    }
#endif

int main(int argc, char **argv) {
    bool use_static_routing(false);

    std::string static_routing_enable("--static-routing");

    for (int i = 1; i < argc; i++) {
        if (static_routing_enable == argv[i]) {
            use_static_routing = true;
        }
    }

    service_sample its_sample(use_static_routing);
#ifndef VSOMEIP_ENABLE_SIGNAL_HANDLING
    its_sample_ptr = &its_sample;
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
#endif
    its_sample.init();
    its_sample.start();

    return 0;
}
