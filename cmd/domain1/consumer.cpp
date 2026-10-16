#include "../../internal/common/kafka/kafka.hpp"
#include <csignal>
#include <atomic>

// Thread-safe break switch for application shutdown loops
std::atomic<bool> keep_running(true);
void signal_handler(int) { keep_running = false; }

int main() {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::string brokers = "127.0.0.1:9092"; // Point to your Redpanda node address
    std::string group = "cpp_transport_group";
    std::vector<std::string> topics = {"orders_topic"};

    try {
        std::cout << "Starting C++ Kafka Transport Consumer layer..." << std::endl;
        KafkaTransportConsumer transport(brokers, group);

        if (!transport.subscribe(topics)) {
            return 1;
        }

        std::cout << "Subscribed and polling network streams. Press Ctrl+C to close." << std::endl;

        while (keep_running) {
            auto message = transport.receive_message(500); // 500ms pool timeout window

            if (message && message->len() > 0) {
                // Safely convert raw buffer payload to text string format
                std::string payload(static_cast<const char*>(message->payload()), message->len());
                std::cout << "Received transport text on [" << message->topic_name()
                          << "] Partition: " << message->partition()
                          << " | Offset: " << message->offset()
                          << " | Payload: " << payload << std::endl;
            }
        }

        std::cout << "Graceful transport termination triggered..." << std::endl;

    } catch (const std::exception& ex) {
        std::cerr << "Fatal network runtime execution fault: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}