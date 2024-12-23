/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "EventHub.h"

#include "UinputDevice.h"

#include <gtest/gtest.h>
#include <inttypes.h>
#include <linux/uinput.h>
#include <log/log.h>
#include <chrono>

#define TAG "EventHub_test"

using android::createUinputDevice;
using android::EventHub;
using android::EventHubInterface;
using android::InputDeviceIdentifier;
using android::RawEvent;
using android::sp;
using android::UinputHomeKey;
using std::chrono_literals::operator""ms;
using std::chrono_literals::operator""s;

static constexpr bool DEBUG = false;

static void dumpEvents(const std::vector<RawEvent>& events) {
    for (const RawEvent& event : events) {
        if (event.type >= EventHubInterface::FIRST_SYNTHETIC_EVENT) {
            switch (event.type) {
                case EventHubInterface::DEVICE_ADDED:
                    ALOGI("Device added: %i", event.deviceId);
                    break;
                case EventHubInterface::DEVICE_REMOVED:
                    ALOGI("Device removed: %i", event.deviceId);
                    break;
            }
        } else {
            ALOGI("Device %" PRId32 " : time = %" PRId64 ", type %i, code %i, value %i",
                  event.deviceId, event.when, event.type, event.code, event.value);
        }
    }
}

// --- EventHubTest ---
class EventHubTest : public testing::Test {
protected:
    std::unique_ptr<EventHubInterface> mEventHub;
    // We are only going to emulate a single input device currently.
    std::unique_ptr<UinputHomeKey> mKeyboard;
    int32_t mDeviceId;

    virtual void SetUp() override {
#if !defined(__ANDROID__)
        GTEST_SKIP() << "It's only possible to interact with uinput on device";
#endif
        mEventHub = std::make_unique<EventHub>();
        consumeInitialDeviceAddedEvents();
        mKeyboard = createUinputDevice<UinputHomeKey>();
        ASSERT_NO_FATAL_FAILURE(mDeviceId = waitForDeviceCreation());
    }
    virtual void TearDown() override {
#if !defined(__ANDROID__)
        return;
#endif
        mKeyboard.reset();
        waitForDeviceClose(mDeviceId);
        assertNoMoreEvents();
    }

    /**
     * Return the device id of the created device.
     */
    int32_t waitForDeviceCreation();
    void waitForDeviceClose(int32_t deviceId);
    void consumeInitialDeviceAddedEvents();
    void assertNoMoreEvents();
    /**
     * Read events from the EventHub.
     *
     * If expectedEvents is set, wait for a significant period of time to try and ensure that
     * the expected number of events has been read. The number of returned events
     * may be smaller (if timeout has been reached) or larger than expectedEvents.
     *
     * If expectedEvents is not set, return all of the immediately available events.
     */
    std::vector<RawEvent> getEvents(std::optional<size_t> expectedEvents = std::nullopt);
};

std::vector<RawEvent> EventHubTest::getEvents(std::optional<size_t> expectedEvents) {
    std::vector<RawEvent> events;

    while (true) {
        std::chrono::milliseconds timeout = 0s;
        if (expectedEvents) {
            timeout = 2s;
        }

        std::vector<RawEvent> newEvents = mEventHub->getEvents(timeout.count());
        if (newEvents.empty()) {
            break;
        }
        events.insert(events.end(), newEvents.begin(), newEvents.end());
        if (expectedEvents && events.size() >= *expectedEvents) {
            break;
        }
    }
    if (DEBUG) {
        dumpEvents(events);
    }
    return events;
}

/**
 * Since the test runs on a real platform, there will be existing devices
 * in addition to the test devices being added. Therefore, when EventHub is first created,
 * it will return a lot of "device added" type of events.
 */
void EventHubTest::consumeInitialDeviceAddedEvents() {
    std::vector<RawEvent> events = getEvents();
    std::set<int32_t /*deviceId*/> existingDevices;
    // All of the events should be DEVICE_ADDED type, except the last one.
    for (size_t i = 0; i < events.size() - 1; i++) {
        const RawEvent& event = events[i];
        EXPECT_EQ(EventHubInterface::DEVICE_ADDED, event.type);
        existingDevices.insert(event.deviceId);
    }
    // None of the existing system devices should be changing while this test is run.
    // Check that the returned device ids are unique for all of the existing devices.
    EXPECT_EQ(existingDevices.size(), events.size() - 1);
}

int32_t EventHubTest::waitForDeviceCreation() {
    // Wait a little longer than usual, to ensure input device has time to be created
    std::vector<RawEvent> events = getEvents(2);
    if (events.size() != 1) {
        ADD_FAILURE() << "Instead of 1 event, received " << events.size();
        return 0; // this value is unused
    }
    const RawEvent& deviceAddedEvent = events[0];
    EXPECT_EQ(static_cast<int32_t>(EventHubInterface::DEVICE_ADDED), deviceAddedEvent.type);
    InputDeviceIdentifier identifier = mEventHub->getDeviceIdentifier(deviceAddedEvent.deviceId);
    const int32_t deviceId = deviceAddedEvent.deviceId;
    EXPECT_EQ(identifier.name, mKeyboard->getName());
    return deviceId;
}

void EventHubTest::waitForDeviceClose(int32_t deviceId) {
    std::vector<RawEvent> events = getEvents(2);
    ASSERT_EQ(1U, events.size());
    const RawEvent& deviceRemovedEvent = events[0];
    EXPECT_EQ(static_cast<int32_t>(EventHubInterface::DEVICE_REMOVED), deviceRemovedEvent.type);
    EXPECT_EQ(deviceId, deviceRemovedEvent.deviceId);
}

void EventHubTest::assertNoMoreEvents() {
    std::vector<RawEvent> events = getEvents();
    ASSERT_TRUE(events.empty());
}

/**
 * Ensure that two identical devices get assigned unique descriptors from EventHub.
 */
TEST_F(EventHubTest, DevicesWithMatchingUniqueIdsAreUnique) {
    std::unique_ptr<UinputHomeKey> keyboard2 = createUinputDevice<UinputHomeKey>();
    int32_t deviceId2;
    ASSERT_NO_FATAL_FAILURE(deviceId2 = waitForDeviceCreation());

    ASSERT_NE(mEventHub->getDeviceIdentifier(mDeviceId).descriptor,
              mEventHub->getDeviceIdentifier(deviceId2).descriptor);
    keyboard2.reset();
    waitForDeviceClose(deviceId2);
}

/**
 * Ensure that input_events are generated with monotonic clock.
 * That means input_event should receive a timestamp that is in the future of the time
 * before the event was sent.
 * Input system uses CLOCK_MONOTONIC everywhere in the code base.
 */
TEST_F(EventHubTest, InputEvent_TimestampIsMonotonic) {
    nsecs_t lastEventTime = systemTime(SYSTEM_TIME_MONOTONIC);
    ASSERT_NO_FATAL_FAILURE(mKeyboard->pressAndReleaseHomeKey());

    std::vector<RawEvent> events = getEvents(4);
    ASSERT_EQ(4U, events.size()) << "Expected to receive 2 keys and 2 syncs, total of 4 events";
    for (const RawEvent& event : events) {
        // Cannot use strict comparison because the events may happen too quickly
        ASSERT_LE(lastEventTime, event.when) << "Event must have occurred after the key was sent";
        ASSERT_LT(std::chrono::nanoseconds(event.when - lastEventTime), 100ms)
                << "Event times are too far apart";
        lastEventTime = event.when; // Ensure all returned events are monotonic
    }
}

// --- BitArrayTest ---
class BitArrayTest : public testing::Test {
protected:
    static constexpr size_t SINGLE_ELE_BITS = 32UL;
    static constexpr size_t MULTI_ELE_BITS = 256UL;

    virtual void SetUp() override {
        mBitmaskSingle.loadFromBuffer(mBufferSingle);
        mBitmaskMulti.loadFromBuffer(mBufferMulti);
    }

    android::BitArray<SINGLE_ELE_BITS> mBitmaskSingle;
    android::BitArray<MULTI_ELE_BITS> mBitmaskMulti;

private:
    const typename android::BitArray<SINGLE_ELE_BITS>::Buffer mBufferSingle = {
            0x800F0F0FUL // bit 0 - 31
    };
    const typename android::BitArray<MULTI_ELE_BITS>::Buffer mBufferMulti = {
            0xFFFFFFFFUL, // bit 0 - 31
            0x01000001UL, // bit 32 - 63
            0x00000000UL, // bit 64 - 95
            0x80000000UL, // bit 96 - 127
            0x00000000UL, // bit 128 - 159
            0x00000000UL, // bit 160 - 191
            0x80000008UL, // bit 192 - 223
            0x00000000UL, // bit 224 - 255
    };
};

TEST_F(BitArrayTest, SetBit) {
    ASSERT_TRUE(mBitmaskSingle.test(0));
    ASSERT_TRUE(mBitmaskSingle.test(31));
    ASSERT_FALSE(mBitmaskSingle.test(7));

    ASSERT_TRUE(mBitmaskMulti.test(32));
    ASSERT_TRUE(mBitmaskMulti.test(56));
    ASSERT_FALSE(mBitmaskMulti.test(192));
    ASSERT_TRUE(mBitmaskMulti.test(223));
    ASSERT_FALSE(mBitmaskMulti.test(255));
}

TEST_F(BitArrayTest, AnyBit) {
    ASSERT_TRUE(mBitmaskSingle.any(31, 32));
    ASSERT_FALSE(mBitmaskSingle.any(12, 16));

    ASSERT_TRUE(mBitmaskMulti.any(31, 32));
    ASSERT_FALSE(mBitmaskMulti.any(33, 33));
    ASSERT_TRUE(mBitmaskMulti.any(32, 55));
    ASSERT_TRUE(mBitmaskMulti.any(33, 57));
    ASSERT_FALSE(mBitmaskMulti.any(33, 55));
    ASSERT_FALSE(mBitmaskMulti.any(130, 190));

    ASSERT_FALSE(mBitmaskMulti.any(128, 195));
    ASSERT_TRUE(mBitmaskMulti.any(128, 196));
    ASSERT_TRUE(mBitmaskMulti.any(128, 224));
    ASSERT_FALSE(mBitmaskMulti.any(255, 256));
}

TEST_F(BitArrayTest, SetBit_InvalidBitIndex) {
    ASSERT_FALSE(mBitmaskSingle.test(32));
    ASSERT_FALSE(mBitmaskMulti.test(256));
}

TEST_F(BitArrayTest, AnyBit_InvalidBitIndex) {
    ASSERT_FALSE(mBitmaskSingle.any(32, 32));
    ASSERT_FALSE(mBitmaskSingle.any(33, 34));

    ASSERT_FALSE(mBitmaskMulti.any(256, 256));
    ASSERT_FALSE(mBitmaskMulti.any(257, 258));
    ASSERT_FALSE(mBitmaskMulti.any(0, 0));
}
