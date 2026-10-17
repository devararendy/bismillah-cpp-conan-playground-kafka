#include "channel.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

TEST(ChannelTest, SendAndReceivePreservesOrder) {
    Channel<int> channel;

    channel.send(10);
    channel.send(20);

    auto first = channel.receive();
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(*first, 10);

    auto second = channel.receive();
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(*second, 20);
}

TEST(ChannelTest, ReceiveWaitsUntilValueIsSent) {
    Channel<int> channel;

    std::thread receiver([&]() {
        auto value = channel.receive();
        ASSERT_TRUE(value.has_value());
        EXPECT_EQ(*value, 42);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    channel.send(42);

    receiver.join();
}

TEST(ChannelTest, ReceiveReturnsNulloptWhenClosedAndEmpty) {
    Channel<int> channel;
    channel.close();

    auto value = channel.receive();
    EXPECT_FALSE(value.has_value());
}

TEST(ChannelTest, SendAfterCloseIsIgnored) {
    Channel<int> channel;
    channel.close();
    channel.send(99);

    auto value = channel.receive();
    EXPECT_FALSE(value.has_value());
}

TEST(ChannelTest, CloseWakesWaitingReceiver) {
    Channel<int> channel;

    std::thread receiver([&]() {
        auto value = channel.receive();
        EXPECT_FALSE(value.has_value());
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    channel.close();

    receiver.join();
}
