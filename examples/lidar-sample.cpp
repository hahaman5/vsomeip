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

#include <vsomeip/vsomeip.hpp>

#include "sample-ids.hpp"

class client_sample {
public:
    client_sample(bool _use_tcp, bool _be_quiet, uint32_t _cycle)
        : app_(vsomeip::runtime::get()->create_application()),
          request_(vsomeip::runtime::get()->create_request(_use_tcp)),
          use_tcp_(_use_tcp),
          be_quiet_(_be_quiet),
          cycle_(_cycle),
          running_(true),
          blocked_(false),
          is_available_(false),
          sender_(std::bind(&client_sample::run, this)),
          offer_(std::bind(&client_sample::run_offer, this)) {
    }

    void init() {
        app_->init();

        std::cout << "Client settings [protocol="
                  << (use_tcp_ ? "TCP" : "UDP")
                  << ":quiet="
                  << (be_quiet_ ? "true" : "false")
                  << ":cycle="
                  << cycle_
                  << "]"
                  << std::endl;

        app_->register_state_handler(
                std::bind(
                    &client_sample::on_state,
                    this,
                    std::placeholders::_1));

        app_->register_message_handler(
                SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID, vsomeip::ANY_METHOD,
                std::bind(&client_sample::on_message,
                          this,
                          std::placeholders::_1));

        request_->set_service(SAMPLE_SERVICE_ID);
        request_->set_instance(SAMPLE_INSTANCE_ID);
        request_->set_method(SAMPLE_METHOD_ID);

        std::shared_ptr< vsomeip::payload > its_payload = vsomeip::runtime::get()->create_payload();
        std::string str = "LIDAR|1212";
        std::vector< vsomeip::byte_t > its_payload_data(str.begin(), str.end());
        its_payload->set_data(its_payload_data);
        request_->set_payload(its_payload);

        app_->register_availability_handler(SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID,
                std::bind(&client_sample::on_availability,
                          this,
                          std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        app_->register_message_handler(
                LIDAR_SERVICE_ID, LIDAR_INSTANCE_ID, vsomeip::ANY_METHOD,
                std::bind(&client_sample::on_message_ctl,
                          this,
                          std::placeholders::_1));
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
        sender_.join();
        app_->stop();
    }
#endif

    void on_state(vsomeip::state_type_e _state) {
        std::cout << "Application " << app_->get_name() << " is "
                << (_state == vsomeip::state_type_e::ST_REGISTERED ?
                        "registered." : "deregistered.")
                << std::endl;
        if (_state == vsomeip::state_type_e::ST_REGISTERED) {
            app_->request_service(SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID);
            blocked_ = true;
            condition_.notify_one();
        }
    }

    void on_availability(vsomeip::service_t _service, vsomeip::instance_t _instance, bool _is_available) {
        std::cout << "Service ["
                << std::setw(4) << std::setfill('0') << std::hex << _service << "." << _instance
                << "] is "
                << (_is_available ? "available." : "NOT available.")
                << std::endl;

        if (SAMPLE_SERVICE_ID == _service && SAMPLE_INSTANCE_ID == _instance) {
            if (is_available_  && !_is_available) {
                is_available_ = false;
            } else if (_is_available && !is_available_) {
                is_available_ = true;
                //send();
            }
        }
    }

    void on_message(const std::shared_ptr< vsomeip::message > &_response) {
        std::cout << "Received a response from Service ["
                << std::setw(4) << std::setfill('0') << std::hex << _response->get_service()
                << "."
                << std::setw(4) << std::setfill('0') << std::hex << _response->get_instance()
                << "] to Client/Session ["
                << std::setw(4) << std::setfill('0') << std::hex << _response->get_client()
                << "/"
                << std::setw(4) << std::setfill('0') << std::hex << _response->get_session()
                << "]"
                << std::endl;
    }


    void run() {
        while (running_) {
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                if (is_available_) {
                    app_->send(request_, true);
                    std::cout << "Client/Session ["
                            << std::setw(4) << std::setfill('0') << std::hex << request_->get_client()
                            << "/"
                            << std::setw(4) << std::setfill('0') << std::hex << request_->get_session()
                            << "] sent a request to Service ["
                            << std::setw(4) << std::setfill('0') << std::hex << request_->get_service()
                            << "."
                            << std::setw(4) << std::setfill('0') << std::hex << request_->get_instance()
                            << "]"
                            << std::endl;
                }

            }
        }
    }

    //member func for control service
    void on_message_ctl(const std::shared_ptr<vsomeip::message> &_request) {
        std::cout << "Received a steering message with Client/Session [" << std::setw(4)
            << std::setfill('0') << std::hex << _request->get_client() << "/"
            << std::setw(4) << std::setfill('0') << std::hex
            << _request->get_session() << "]"
            << std::endl;

		std::shared_ptr< vsomeip::payload > its_payload = _request->get_payload();
		vsomeip::byte_t *arr= its_payload->get_data();

        std::shared_ptr<vsomeip::message> its_response
            = vsomeip::runtime::get()->create_response(_request);

        std::shared_ptr<vsomeip::payload> resp_payload
            = vsomeip::runtime::get()->create_payload();
        std::string str("192.168.50.111:12345"); //edit as appropriate address
        std::vector<vsomeip::byte_t> its_payload_data(str.begin(), str.end());
        its_payload_data.push_back('\0');
        resp_payload->set_data(its_payload_data);
        its_response->set_payload(resp_payload);

        app_->send(its_response, true);
    }


    void run_offer() {
        std::unique_lock<std::mutex> its_lock(mutex_);
        while (!blocked_)
            condition_.wait(its_lock);

        app_->offer_service(LIDAR_SERVICE_ID, LIDAR_INSTANCE_ID);
        while (running_);
    }
private:
    std::shared_ptr< vsomeip::application > app_;
    std::shared_ptr< vsomeip::message > request_;
    bool use_tcp_;
    bool be_quiet_;
    uint32_t cycle_;
    vsomeip::session_t session_;
    std::mutex mutex_;
    std::condition_variable condition_;
    bool running_;
    bool blocked_;
    bool is_available_;
    std::map<int, std::string> dev_map;

    std::thread sender_;
    std::thread offer_;

    //members for control service
};

#ifndef VSOMEIP_ENABLE_SIGNAL_HANDLING
    client_sample *its_sample_ptr(nullptr);
    void handle_signal(int _signal) {
        if (its_sample_ptr != nullptr &&
                (_signal == SIGINT || _signal == SIGTERM))
            its_sample_ptr->stop();
    }
#endif

int main(int argc, char **argv) {
    bool use_tcp = false;
    bool be_quiet = false;
    uint32_t cycle = 1000; // Default: 1s

    std::string tcp_enable("--tcp");
    std::string udp_enable("--udp");
    std::string quiet_enable("--quiet");
    std::string cycle_arg("--cycle");

    int i = 1;
    while (i < argc) {
        if (tcp_enable == argv[i]) {
            use_tcp = true;
        } else if (udp_enable == argv[i]) {
            use_tcp = false;
        } else if (quiet_enable == argv[i]) {
            be_quiet = true;
        } else if (cycle_arg == argv[i] && i+1 < argc) {
            i++;
            std::stringstream converter;
            converter << argv[i];
            converter >> cycle;
        }
        i++;
    }

    client_sample its_sample(use_tcp, be_quiet, cycle);
#ifndef VSOMEIP_ENABLE_SIGNAL_HANDLING
    its_sample_ptr = &its_sample;
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
#endif
    its_sample.init();
    its_sample.start();
    return 0;
}
