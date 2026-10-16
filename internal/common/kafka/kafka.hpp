#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include <librdkafka/rdkafkacpp.h>

// Custom event callback class to catch errors and log status changes from Redpanda
class ConsumerEventCb : public RdKafka::EventCb {
public:
    void event_cb(RdKafka::Event &event) override {
        switch (event.type()) {
            case RdKafka::Event::EVENT_ERROR:
                std::cerr << "[Kafka Error] (" << RdKafka::err2str(event.err()) << "): " << event.str() << std::endl;
                break;
            case RdKafka::Event::EVENT_STATS:
                // Periodically receives cluster metrics (if statistics.interval.ms is set)
                break;
            case RdKafka::Event::EVENT_LOG:
                std::cout << "[Kafka Log] " << event.str() << std::endl;
                break;
            default:
                break;
        }
    }
};

class KafkaTransportConsumer {
public:
    KafkaTransportConsumer(const std::string& bootstrap_servers, const std::string& group_id) {
        if (bootstrap_servers.empty()) {
            throw std::runtime_error("bootstrap_servers must not be empty");
        }

        std::string errstr;

        // 1. Create global configuration
        global_conf_.reset(RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));
        
        // Register custom event callbacks for connection tracking
        if (global_conf_->set("event_cb", &event_cb_, errstr) != RdKafka::Conf::CONF_OK) {
            throw std::runtime_error("Failed to set event callback: " + errstr);
        }

        // Configure fundamental Redpanda connection strings
        if (global_conf_->set("bootstrap.servers", bootstrap_servers, errstr) != RdKafka::Conf::CONF_OK ||
            global_conf_->set("group.id", group_id, errstr) != RdKafka::Conf::CONF_OK ||
            global_conf_->set("auto.offset.reset", "earliest", errstr) != RdKafka::Conf::CONF_OK ||
            global_conf_->set("enable.auto.commit", "true", errstr) != RdKafka::Conf::CONF_OK) {
            throw std::runtime_error("Kafka transport configuration failure: " + errstr);
        }

        // 2. Instantiate the managed consumer engine
        consumer_.reset(RdKafka::KafkaConsumer::create(global_conf_.get(), errstr));
        if (!consumer_) {
            throw std::runtime_error("Failed to initialize consumer client: " + errstr);
        }
    }

    ~KafkaTransportConsumer() {
        // Explicitly close the consumer down cleanly to release cluster rebalance locks
        if (consumer_) {
            consumer_->close();
        }
        // librdkafka global cleanups are handled natively when instances destroy
    }

    // Subscribe the client transport layer to specific topics
    bool subscribe(const std::vector<std::string>& topics) {
        RdKafka::ErrorCode err = consumer_->subscribe(topics);
        if (err != RdKafka::ERR_NO_ERROR) {
            std::cerr << "Failed to subscribe to topics: " << RdKafka::err2str(err) << std::endl;
            return false;
        }
        return true;
    }

    // Single-message consumption poll wrapper loop
    std::unique_ptr<RdKafka::Message> receive_message(int timeout_ms = 1000) {
        // Blocks or polls for incoming buffered events from network channels
        std::unique_ptr<RdKafka::Message> msg(consumer_->consume(timeout_ms));
        if (!msg) {
            return nullptr;
        }
        
        switch (msg->err()) {
            case RdKafka::ERR_NO_ERROR:
                // Valid payload ready for reading
                return msg;

            case RdKafka::ERR__TIMED_OUT:
                // Safe heartbeat timeout; no data available yet
                return nullptr;

            case RdKafka::ERR__PARTITION_EOF:
                // End of partition metadata tracking marker reached
                return nullptr;

            default:
                // Errors or dynamic topic changes occurring
                std::cerr << "Consumption layer issue encountered: " << msg->errstr() << std::endl;
                return nullptr;
        }
    }

private:
    std::unique_ptr<RdKafka::Conf> global_conf_;
    std::unique_ptr<RdKafka::KafkaConsumer> consumer_;
    ConsumerEventCb event_cb_;
};
