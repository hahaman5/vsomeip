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
#include <utility>
#include <boost/asio.hpp>

#include "sample-ids.hpp"

using boost::asio::ip::tcp;

static bool is_example;

class service_sample {
public:
    service_sample(bool _use_static_routing) :
            app_(vsomeip::runtime::get()->create_application()),
            is_registered_(false),
            request_(vsomeip::runtime::get()->create_request(false)),
            use_static_routing_(_use_static_routing),
            blocked_(false),
            running_(true),
            is_available_(false),
            steer_angle(0),
            offer_thread_(std::bind(&service_sample::run, this)),
            thr_sender_(std::bind(&service_sample::sender, this)),
            dev_send_thread_(std::bind(&service_sample::send_dev_map, this)), 
            dev_mgr_thread_(std::bind(&service_sample::run_map_mgr, this)) {
    }

    void init() {
        std::lock_guard<std::mutex> its_lock(mutex_);

        app_->init();

        if(is_example){
            std::string dev_examples[] = {"example1|aaaa", "example2|bbbb", "example3|cccc", "example4|dddd"};

            for(int i=0;i<4;++i){
                std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
                now += std::chrono::seconds(30+10 * i);
                dev_map[dev_examples[i]] = now;
            }
        }


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
        request_->set_service(CONTROL_SERVICE_ID);
        request_->set_instance(CONTROL_INSTANCE_ID);
        request_->set_method(CONTROL_METHOD_ID);

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
    }

    void stop_offer() {
        app_->stop_offer_service(SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID);
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
                condition_map.notify_one();
            }
            app_->request_service(CONTROL_SERVICE_ID, CONTROL_INSTANCE_ID);
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

        std::cout << "Received dev name|ID: " <<  str << std::endl;
		
        std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
        now += std::chrono::seconds(5);
        dev_map[str] = now;
        //send_dev_map();

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
        std::this_thread::sleep_for(std::chrono::milliseconds(5000));
        while(1){
            std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
            bool is_mod = false;

            for(auto it = dev_map.begin(); it != dev_map.end();) {
                if(it->second <= now){
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    std::cout << it->first << " is timed out" << std::endl;
                    it = dev_map.erase(it);
                    is_mod = true;
                    it++;
                }else{
                    it++;
                }
                std::cout << " ";
            }

            if(is_mod){
                //send_dev_map();
                is_mod = false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }

    }

    void send_dev_map() {
        /* TODO: write some code to send dev_map to head unit app */
        tcp::acceptor a(io_service_dev, tcp::endpoint(tcp::v4(), 5689));
        for (;;)
        {
            tcp::socket sock(io_service_dev);
            a.accept(sock);
            while(true){
                char data[100];

                boost::system::error_code error;
                size_t length = sock.read_some(boost::asio::buffer(data), error);

                if (error == boost::asio::error::eof)
                    break; // Connection closed cleanly by peer.
                else if (error)
                    continue;

                std::cout << "sending list\n";
                int len = dev_map.size();

                std::stringstream ss;
                ss << len << std::endl;
                for(auto it = dev_map.begin(); it != dev_map.end();) {
                    ss << it->first << std::endl;
                    std::cout << "element : " << it->first << std::endl;
                    it++;
                }
                ss << "FIN" << std::endl;

                std::string str(ss.str());
                std::cout << "sending list\n" << str;

                boost::asio::write(sock, boost::asio::buffer(str.c_str(), str.size()));
                    
            }
        }
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
        tcp::acceptor a(io_service, tcp::endpoint(tcp::v4(), 5688));
        for (;;)
        {
            tcp::socket sock(io_service);
            a.accept(sock);
            bool conn = true;
            while(conn){
                char data[10];

                boost::system::error_code error;
                size_t length = sock.read_some(boost::asio::buffer(data), error);

                if (error == boost::asio::error::eof){
                    conn = false;
                    //break; // Connection closed cleanly by peer.
                }
                else if (error){

                    std::cout << "something err\n";
                    continue;
                }

                //std::cout << "packet len: " << length << std::endl;
                if(length < 4){
                    continue;
                }


                steer_angle = *((int *)data);

                std::cout << "steering angle is " << std::dec<< steer_angle << std::endl;

                if(is_available_ && old_val != steer_angle){
                    std::shared_ptr< vsomeip::payload > its_payload = vsomeip::runtime::get()->create_payload();
                    unsigned char bytes[sizeof steer_angle];

                    int angle = steer_angle;
                    for(int i=sizeof steer_angle; i>0; --i){
                        bytes[i-1] = (unsigned char)(angle & 0xff);
                        angle = angle >> 8;
                    }

                    for(int i=0;i < sizeof steer_angle; ++i)
                        std::cout << std::setw(2) << std::setfill('0') << std::hex << (unsigned int)bytes[i] << " ";
                    std::cout << std::endl;

                    its_payload->set_data(bytes, sizeof steer_angle);
                    request_->set_payload(its_payload);
                    app_->send(request_, true);
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    old_val = steer_angle;
                }else{

                    /*
                       std::this_thread::sleep_for(std::chrono::milliseconds(5000));
                       steer_angle +=100;
                       if(steer_angle > 500) steer_angle = -500;
                       */
                }

            }
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
    boost::asio::io_service io_service_dev;
    std::thread dev_send_thread_;

    // members for steering request
    std::shared_ptr< vsomeip::message > request_;
    bool is_available_;
    std::thread thr_sender_;
    int steer_angle;
    std::condition_variable condition_map;
    boost::asio::io_service io_service;

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
    std::string opt_example("--example");

    is_example = false;
    for (int i = 1; i < argc; i++) {
        if (static_routing_enable == argv[i]) {
            use_static_routing = true;
        } else if (opt_example == argv[i]) {
            is_example = true;
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
