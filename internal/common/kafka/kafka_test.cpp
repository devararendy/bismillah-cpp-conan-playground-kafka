#include "kafka.hpp"
#include <gtest/gtest.h>

TEST(KafkaTransportConsumerTest, RejectsAnEmptyBrokerList) {
    EXPECT_THROW(
        KafkaTransportConsumer("", "kafka-unit-test-group"),
        std::runtime_error);
}

TEST(KafkaTransportConsumerTest, SubscribesAndPollsWithoutRunningBroker) {
    KafkaTransportConsumer consumer("127.0.0.1:1", "kafka-unit-test-group");

    ASSERT_TRUE(consumer.subscribe({"kafka-unit-test-topic"}));
    EXPECT_FALSE(consumer.receive_message(0));
}