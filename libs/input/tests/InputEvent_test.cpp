/*
 * Copyright (C) 2011 The Android Open Source Project
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

#include <array>
#include <math.h>

#include <android-base/properties.h>
#include <attestation/HmacKeyManager.h>
#include <binder/Parcel.h>
#include <gtest/gtest.h>
#include <input/Input.h>
#include <input/InputEventBuilders.h>

namespace android {

namespace {

// Default display id.
constexpr ui::LogicalDisplayId DISPLAY_ID = ui::LogicalDisplayId::DEFAULT;

constexpr float EPSILON = MotionEvent::ROUNDING_PRECISION;

constexpr auto POINTER_0_DOWN =
        AMOTION_EVENT_ACTION_POINTER_DOWN | (0 << AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT);

constexpr auto POINTER_1_DOWN =
        AMOTION_EVENT_ACTION_POINTER_DOWN | (1 << AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT);

constexpr auto POINTER_0_UP =
        AMOTION_EVENT_ACTION_POINTER_UP | (0 << AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT);

constexpr auto POINTER_1_UP =
        AMOTION_EVENT_ACTION_POINTER_UP | (1 << AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT);

std::array<float, 9> asFloat9(const ui::Transform& t) {
    std::array<float, 9> mat{};
    mat[0] = t[0][0];
    mat[1] = t[1][0];
    mat[2] = t[2][0];
    mat[3] = t[0][1];
    mat[4] = t[1][1];
    mat[5] = t[2][1];
    mat[6] = t[0][2];
    mat[7] = t[1][2];
    mat[8] = t[2][2];
    return mat;
}

class BaseTest : public testing::Test {
protected:
    static constexpr std::array<uint8_t, 32> HMAC = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10,
                                                     11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
                                                     22, 23, 24, 25, 26, 27, 28, 29, 30, 31};
};

} // namespace

// --- PointerCoordsTest ---

class PointerCoordsTest : public BaseTest {
};

TEST_F(PointerCoordsTest, ClearSetsBitsToZero) {
    PointerCoords coords;
    coords.clear();

    ASSERT_EQ(0ULL, coords.bits);
    ASSERT_FALSE(coords.isResampled);
}

TEST_F(PointerCoordsTest, AxisValues) {
    PointerCoords coords;
    coords.clear();

    // Check invariants when no axes are present.
    ASSERT_EQ(0, coords.getAxisValue(0))
            << "getAxisValue should return zero because axis is not present";
    ASSERT_EQ(0, coords.getAxisValue(1))
            << "getAxisValue should return zero because axis is not present";

    // Set first axis.
    ASSERT_EQ(OK, coords.setAxisValue(1, 5));
    ASSERT_EQ(5, coords.values[0]);
    ASSERT_EQ(0x4000000000000000ULL, coords.bits);

    ASSERT_EQ(0, coords.getAxisValue(0))
            << "getAxisValue should return zero because axis is not present";
    ASSERT_EQ(5, coords.getAxisValue(1))
            << "getAxisValue should return value of axis";

    // Set an axis with a higher id than all others.  (appending value at the end)
    ASSERT_EQ(OK, coords.setAxisValue(3, 2));
    ASSERT_EQ(0x5000000000000000ULL, coords.bits);
    ASSERT_EQ(5, coords.values[0]);
    ASSERT_EQ(2, coords.values[1]);

    ASSERT_EQ(0, coords.getAxisValue(0))
            << "getAxisValue should return zero because axis is not present";
    ASSERT_EQ(5, coords.getAxisValue(1))
            << "getAxisValue should return value of axis";
    ASSERT_EQ(0, coords.getAxisValue(2))
            << "getAxisValue should return zero because axis is not present";
    ASSERT_EQ(2, coords.getAxisValue(3))
            << "getAxisValue should return value of axis";

    // Set an axis with an id lower than all others.  (prepending value at beginning)
    ASSERT_EQ(OK, coords.setAxisValue(0, 4));
    ASSERT_EQ(0xd000000000000000ULL, coords.bits);
    ASSERT_EQ(4, coords.values[0]);
    ASSERT_EQ(5, coords.values[1]);
    ASSERT_EQ(2, coords.values[2]);

    ASSERT_EQ(4, coords.getAxisValue(0))
            << "getAxisValue should return value of axis";
    ASSERT_EQ(5, coords.getAxisValue(1))
            << "getAxisValue should return value of axis";
    ASSERT_EQ(0, coords.getAxisValue(2))
            << "getAxisValue should return zero because axis is not present";
    ASSERT_EQ(2, coords.getAxisValue(3))
            << "getAxisValue should return value of axis";

    // Set an axis with an id between the others.  (inserting value in the middle)
    ASSERT_EQ(OK, coords.setAxisValue(2, 1));
    ASSERT_EQ(0xf000000000000000ULL, coords.bits);
    ASSERT_EQ(4, coords.values[0]);
    ASSERT_EQ(5, coords.values[1]);
    ASSERT_EQ(1, coords.values[2]);
    ASSERT_EQ(2, coords.values[3]);

    ASSERT_EQ(4, coords.getAxisValue(0))
            << "getAxisValue should return value of axis";
    ASSERT_EQ(5, coords.getAxisValue(1))
            << "getAxisValue should return value of axis";
    ASSERT_EQ(1, coords.getAxisValue(2))
            << "getAxisValue should return value of axis";
    ASSERT_EQ(2, coords.getAxisValue(3))
            << "getAxisValue should return value of axis";

    // Set an existing axis value in place.
    ASSERT_EQ(OK, coords.setAxisValue(1, 6));
    ASSERT_EQ(0xf000000000000000ULL, coords.bits);
    ASSERT_EQ(4, coords.values[0]);
    ASSERT_EQ(6, coords.values[1]);
    ASSERT_EQ(1, coords.values[2]);
    ASSERT_EQ(2, coords.values[3]);

    ASSERT_EQ(4, coords.getAxisValue(0))
            << "getAxisValue should return value of axis";
    ASSERT_EQ(6, coords.getAxisValue(1))
            << "getAxisValue should return value of axis";
    ASSERT_EQ(1, coords.getAxisValue(2))
            << "getAxisValue should return value of axis";
    ASSERT_EQ(2, coords.getAxisValue(3))
            << "getAxisValue should return value of axis";

    // Set maximum number of axes.
    for (size_t axis = 4; axis < PointerCoords::MAX_AXES; axis++) {
        ASSERT_EQ(OK, coords.setAxisValue(axis, axis));
    }
    ASSERT_EQ(PointerCoords::MAX_AXES, __builtin_popcountll(coords.bits));

    // Try to set one more axis beyond maximum number.
    // Ensure bits are unchanged.
    ASSERT_EQ(NO_MEMORY, coords.setAxisValue(PointerCoords::MAX_AXES, 100));
    ASSERT_EQ(PointerCoords::MAX_AXES, __builtin_popcountll(coords.bits));
}

TEST_F(PointerCoordsTest, Parcel) {
    Parcel parcel;

    PointerCoords inCoords;
    inCoords.clear();
    PointerCoords outCoords;

    // Round trip with empty coords.
    inCoords.writeToParcel(&parcel);
    parcel.setDataPosition(0);
    outCoords.readFromParcel(&parcel);

    ASSERT_EQ(0ULL, outCoords.bits);
    ASSERT_FALSE(outCoords.isResampled);

    // Round trip with some values.
    parcel.freeData();
    inCoords.setAxisValue(2, 5);
    inCoords.setAxisValue(5, 8);
    inCoords.isResampled = true;

    inCoords.writeToParcel(&parcel);
    parcel.setDataPosition(0);
    outCoords.readFromParcel(&parcel);

    ASSERT_EQ(outCoords.bits, inCoords.bits);
    ASSERT_EQ(outCoords.values[0], inCoords.values[0]);
    ASSERT_EQ(outCoords.values[1], inCoords.values[1]);
    ASSERT_TRUE(outCoords.isResampled);
}


// --- KeyEventTest ---

class KeyEventTest : public BaseTest {
};

TEST_F(KeyEventTest, Properties) {
    KeyEvent event;

    // Initialize and get properties.
    constexpr nsecs_t ARBITRARY_DOWN_TIME = 1;
    constexpr nsecs_t ARBITRARY_EVENT_TIME = 2;
    const int32_t id = InputEvent::nextId();
    event.initialize(id, 2, AINPUT_SOURCE_GAMEPAD, DISPLAY_ID, HMAC, AKEY_EVENT_ACTION_DOWN,
                     AKEY_EVENT_FLAG_FROM_SYSTEM, AKEYCODE_BUTTON_X, 121, AMETA_ALT_ON, 1,
                     ARBITRARY_DOWN_TIME, ARBITRARY_EVENT_TIME);

    ASSERT_EQ(id, event.getId());
    ASSERT_EQ(InputEventType::KEY, event.getType());
    ASSERT_EQ(2, event.getDeviceId());
    ASSERT_EQ(AINPUT_SOURCE_GAMEPAD, event.getSource());
    ASSERT_EQ(DISPLAY_ID, event.getDisplayId());
    EXPECT_EQ(HMAC, event.getHmac());
    ASSERT_EQ(AKEY_EVENT_ACTION_DOWN, event.getAction());
    ASSERT_EQ(AKEY_EVENT_FLAG_FROM_SYSTEM, event.getFlags());
    ASSERT_EQ(AKEYCODE_BUTTON_X, event.getKeyCode());
    ASSERT_EQ(121, event.getScanCode());
    ASSERT_EQ(AMETA_ALT_ON, event.getMetaState());
    ASSERT_EQ(1, event.getRepeatCount());
    ASSERT_EQ(ARBITRARY_DOWN_TIME, event.getDownTime());
    ASSERT_EQ(ARBITRARY_EVENT_TIME, event.getEventTime());

    // Set source.
    event.setSource(AINPUT_SOURCE_JOYSTICK);
    ASSERT_EQ(AINPUT_SOURCE_JOYSTICK, event.getSource());

    // Set display id.
    constexpr ui::LogicalDisplayId newDisplayId = ui::LogicalDisplayId{2};
    event.setDisplayId(newDisplayId);
    ASSERT_EQ(newDisplayId, event.getDisplayId());
}


// --- MotionEventTest ---

class MotionEventTest : public BaseTest {
protected:
    static constexpr nsecs_t ARBITRARY_DOWN_TIME = 1;
    static constexpr nsecs_t ARBITRARY_EVENT_TIME = 2;
    static constexpr float X_SCALE = 2.0;
    static constexpr float Y_SCALE = 3.0;
    static constexpr float X_OFFSET = 1;
    static constexpr float Y_OFFSET = 1.1;
    static constexpr float RAW_X_SCALE = 4.0;
    static constexpr float RAW_Y_SCALE = -5.0;
    static constexpr float RAW_X_OFFSET = 12;
    static constexpr float RAW_Y_OFFSET = -41.1;

    void SetUp() override;

    int32_t mId;
    ui::Transform mTransform;
    ui::Transform mRawTransform;
    PointerProperties mPointerProperties[2];
    struct Sample {
        PointerCoords pointerCoords[2];
    };
    std::array<Sample, 3> mSamples{};

    void initializeEventWithHistory(MotionEvent* event);
    void assertEqualsEventWithHistory(const MotionEvent* event);
};

void MotionEventTest::SetUp() {
    mId = InputEvent::nextId();
    mTransform.set({X_SCALE, 0, X_OFFSET, 0, Y_SCALE, Y_OFFSET, 0, 0, 1});
    mRawTransform.set({RAW_X_SCALE, 0, RAW_X_OFFSET, 0, RAW_Y_SCALE, RAW_Y_OFFSET, 0, 0, 1});

    mPointerProperties[0].clear();
    mPointerProperties[0].id = 1;
    mPointerProperties[0].toolType = ToolType::FINGER;
    mPointerProperties[1].clear();
    mPointerProperties[1].id = 2;
    mPointerProperties[1].toolType = ToolType::STYLUS;

    mSamples[0].pointerCoords[0].clear();
    mSamples[0].pointerCoords[0].setAxisValue(AMOTION_EVENT_AXIS_X, 10);
    mSamples[0].pointerCoords[0].setAxisValue(AMOTION_EVENT_AXIS_Y, 11);
    mSamples[0].pointerCoords[0].setAxisValue(AMOTION_EVENT_AXIS_PRESSURE, 12);
    mSamples[0].pointerCoords[0].setAxisValue(AMOTION_EVENT_AXIS_SIZE, 13);
    mSamples[0].pointerCoords[0].setAxisValue(AMOTION_EVENT_AXIS_TOUCH_MAJOR, 14);
    mSamples[0].pointerCoords[0].setAxisValue(AMOTION_EVENT_AXIS_TOUCH_MINOR, 15);
    mSamples[0].pointerCoords[0].setAxisValue(AMOTION_EVENT_AXIS_TOOL_MAJOR, 16);
    mSamples[0].pointerCoords[0].setAxisValue(AMOTION_EVENT_AXIS_TOOL_MINOR, 17);
    mSamples[0].pointerCoords[0].setAxisValue(AMOTION_EVENT_AXIS_ORIENTATION, 18);
    mSamples[0].pointerCoords[0].isResampled = true;
    mSamples[0].pointerCoords[1].clear();
    mSamples[0].pointerCoords[1].setAxisValue(AMOTION_EVENT_AXIS_X, 20);
    mSamples[0].pointerCoords[1].setAxisValue(AMOTION_EVENT_AXIS_Y, 21);
    mSamples[0].pointerCoords[1].setAxisValue(AMOTION_EVENT_AXIS_PRESSURE, 22);
    mSamples[0].pointerCoords[1].setAxisValue(AMOTION_EVENT_AXIS_SIZE, 23);
    mSamples[0].pointerCoords[1].setAxisValue(AMOTION_EVENT_AXIS_TOUCH_MAJOR, 24);
    mSamples[0].pointerCoords[1].setAxisValue(AMOTION_EVENT_AXIS_TOUCH_MINOR, 25);
    mSamples[0].pointerCoords[1].setAxisValue(AMOTION_EVENT_AXIS_TOOL_MAJOR, 26);
    mSamples[0].pointerCoords[1].setAxisValue(AMOTION_EVENT_AXIS_TOOL_MINOR, 27);
    mSamples[0].pointerCoords[1].setAxisValue(AMOTION_EVENT_AXIS_ORIENTATION, 28);

    mSamples[1].pointerCoords[0].clear();
    mSamples[1].pointerCoords[0].setAxisValue(AMOTION_EVENT_AXIS_X, 110);
    mSamples[1].pointerCoords[0].setAxisValue(AMOTION_EVENT_AXIS_Y, 111);
    mSamples[1].pointerCoords[0].setAxisValue(AMOTION_EVENT_AXIS_PRESSURE, 112);
    mSamples[1].pointerCoords[0].setAxisValue(AMOTION_EVENT_AXIS_SIZE, 113);
    mSamples[1].pointerCoords[0].setAxisValue(AMOTION_EVENT_AXIS_TOUCH_MAJOR, 114);
    mSamples[1].pointerCoords[0].setAxisValue(AMOTION_EVENT_AXIS_TOUCH_MINOR, 115);
    mSamples[1].pointerCoords[0].setAxisValue(AMOTION_EVENT_AXIS_TOOL_MAJOR, 116);
    mSamples[1].pointerCoords[0].setAxisValue(AMOTION_EVENT_AXIS_TOOL_MINOR, 117);
    mSamples[1].pointerCoords[0].setAxisValue(AMOTION_EVENT_AXIS_ORIENTATION, 118);
    mSamples[1].pointerCoords[0].isResampled = true;
    mSamples[1].pointerCoords[1].clear();
    mSamples[1].pointerCoords[1].setAxisValue(AMOTION_EVENT_AXIS_X, 120);
    mSamples[1].pointerCoords[1].setAxisValue(AMOTION_EVENT_AXIS_Y, 121);
    mSamples[1].pointerCoords[1].setAxisValue(AMOTION_EVENT_AXIS_PRESSURE, 122);
    mSamples[1].pointerCoords[1].setAxisValue(AMOTION_EVENT_AXIS_SIZE, 123);
    mSamples[1].pointerCoords[1].setAxisValue(AMOTION_EVENT_AXIS_TOUCH_MAJOR, 124);
    mSamples[1].pointerCoords[1].setAxisValue(AMOTION_EVENT_AXIS_TOUCH_MINOR, 125);
    mSamples[1].pointerCoords[1].setAxisValue(AMOTION_EVENT_AXIS_TOOL_MAJOR, 126);
    mSamples[1].pointerCoords[1].setAxisValue(AMOTION_EVENT_AXIS_TOOL_MINOR, 127);
    mSamples[1].pointerCoords[1].setAxisValue(AMOTION_EVENT_AXIS_ORIENTATION, 128);
    mSamples[1].pointerCoords[1].isResampled = true;

    mSamples[2].pointerCoords[0].clear();
    mSamples[2].pointerCoords[0].setAxisValue(AMOTION_EVENT_AXIS_X, 210);
    mSamples[2].pointerCoords[0].setAxisValue(AMOTION_EVENT_AXIS_Y, 211);
    mSamples[2].pointerCoords[0].setAxisValue(AMOTION_EVENT_AXIS_PRESSURE, 212);
    mSamples[2].pointerCoords[0].setAxisValue(AMOTION_EVENT_AXIS_SIZE, 213);
    mSamples[2].pointerCoords[0].setAxisValue(AMOTION_EVENT_AXIS_TOUCH_MAJOR, 214);
    mSamples[2].pointerCoords[0].setAxisValue(AMOTION_EVENT_AXIS_TOUCH_MINOR, 215);
    mSamples[2].pointerCoords[0].setAxisValue(AMOTION_EVENT_AXIS_TOOL_MAJOR, 216);
    mSamples[2].pointerCoords[0].setAxisValue(AMOTION_EVENT_AXIS_TOOL_MINOR, 217);
    mSamples[2].pointerCoords[0].setAxisValue(AMOTION_EVENT_AXIS_ORIENTATION, 218);
    mSamples[2].pointerCoords[1].clear();
    mSamples[2].pointerCoords[1].setAxisValue(AMOTION_EVENT_AXIS_X, 220);
    mSamples[2].pointerCoords[1].setAxisValue(AMOTION_EVENT_AXIS_Y, 221);
    mSamples[2].pointerCoords[1].setAxisValue(AMOTION_EVENT_AXIS_PRESSURE, 222);
    mSamples[2].pointerCoords[1].setAxisValue(AMOTION_EVENT_AXIS_SIZE, 223);
    mSamples[2].pointerCoords[1].setAxisValue(AMOTION_EVENT_AXIS_TOUCH_MAJOR, 224);
    mSamples[2].pointerCoords[1].setAxisValue(AMOTION_EVENT_AXIS_TOUCH_MINOR, 225);
    mSamples[2].pointerCoords[1].setAxisValue(AMOTION_EVENT_AXIS_TOOL_MAJOR, 226);
    mSamples[2].pointerCoords[1].setAxisValue(AMOTION_EVENT_AXIS_TOOL_MINOR, 227);
    mSamples[2].pointerCoords[1].setAxisValue(AMOTION_EVENT_AXIS_ORIENTATION, 228);
}

void MotionEventTest::initializeEventWithHistory(MotionEvent* event) {
    const int32_t flags = AMOTION_EVENT_FLAG_WINDOW_IS_OBSCURED |
            AMOTION_EVENT_PRIVATE_FLAG_SUPPORTS_ORIENTATION |
            AMOTION_EVENT_PRIVATE_FLAG_SUPPORTS_DIRECTIONAL_ORIENTATION;
    event->initialize(mId, 2, AINPUT_SOURCE_TOUCHSCREEN, DISPLAY_ID, HMAC,
                      AMOTION_EVENT_ACTION_MOVE, 0, flags, AMOTION_EVENT_EDGE_FLAG_TOP,
                      AMETA_ALT_ON, AMOTION_EVENT_BUTTON_PRIMARY, MotionClassification::NONE,
                      mTransform, 2.0f, 2.1f, AMOTION_EVENT_INVALID_CURSOR_POSITION,
                      AMOTION_EVENT_INVALID_CURSOR_POSITION, mRawTransform, ARBITRARY_DOWN_TIME,
                      ARBITRARY_EVENT_TIME, 2, mPointerProperties, mSamples[0].pointerCoords);
    event->addSample(ARBITRARY_EVENT_TIME + 1, mSamples[1].pointerCoords, event->getId());
    event->addSample(ARBITRARY_EVENT_TIME + 2, mSamples[2].pointerCoords, event->getId());
}

void MotionEventTest::assertEqualsEventWithHistory(const MotionEvent* event) {
    // Check properties.
    ASSERT_EQ(mId, event->getId());
    ASSERT_EQ(InputEventType::MOTION, event->getType());
    ASSERT_EQ(2, event->getDeviceId());
    ASSERT_EQ(AINPUT_SOURCE_TOUCHSCREEN, event->getSource());
    ASSERT_EQ(DISPLAY_ID, event->getDisplayId());
    EXPECT_EQ(HMAC, event->getHmac());
    ASSERT_EQ(AMOTION_EVENT_ACTION_MOVE, event->getAction());
    ASSERT_EQ(AMOTION_EVENT_FLAG_WINDOW_IS_OBSCURED |
                      AMOTION_EVENT_PRIVATE_FLAG_SUPPORTS_ORIENTATION |
                      AMOTION_EVENT_PRIVATE_FLAG_SUPPORTS_DIRECTIONAL_ORIENTATION,
              event->getFlags());
    ASSERT_EQ(AMOTION_EVENT_EDGE_FLAG_TOP, event->getEdgeFlags());
    ASSERT_EQ(AMETA_ALT_ON, event->getMetaState());
    ASSERT_EQ(AMOTION_EVENT_BUTTON_PRIMARY, event->getButtonState());
    ASSERT_EQ(MotionClassification::NONE, event->getClassification());
    EXPECT_EQ(mTransform, event->getTransform());
    ASSERT_NEAR((-RAW_X_OFFSET / RAW_X_SCALE) * X_SCALE + X_OFFSET, event->getRawXOffset(),
                EPSILON);
    ASSERT_NEAR((-RAW_Y_OFFSET / RAW_Y_SCALE) * Y_SCALE + Y_OFFSET, event->getRawYOffset(),
                EPSILON);
    ASSERT_EQ(2.0f, event->getXPrecision());
    ASSERT_EQ(2.1f, event->getYPrecision());
    ASSERT_EQ(ARBITRARY_DOWN_TIME, event->getDownTime());

    ASSERT_EQ(2U, event->getPointerCount());
    ASSERT_EQ(1, event->getPointerId(0));
    ASSERT_EQ(ToolType::FINGER, event->getToolType(0));
    ASSERT_EQ(2, event->getPointerId(1));
    ASSERT_EQ(ToolType::STYLUS, event->getToolType(1));

    ASSERT_EQ(2U, event->getHistorySize());

    // Check data.
    ASSERT_EQ(ARBITRARY_EVENT_TIME, event->getHistoricalEventTime(0));
    ASSERT_EQ(ARBITRARY_EVENT_TIME + 1, event->getHistoricalEventTime(1));
    ASSERT_EQ(ARBITRARY_EVENT_TIME + 2, event->getEventTime());

    // Ensure the underlying PointerCoords are identical.
    for (int sampleIdx = 0; sampleIdx < 3; sampleIdx++) {
        for (int pointerIdx = 0; pointerIdx < 2; pointerIdx++) {
            ASSERT_EQ(mSamples[sampleIdx].pointerCoords[pointerIdx],
                      event->getSamplePointerCoords()[sampleIdx * 2 + pointerIdx]);
        }
    }

    ASSERT_NEAR(11, event->getHistoricalRawPointerCoords(0, 0)->getAxisValue(AMOTION_EVENT_AXIS_Y),
                EPSILON);
    ASSERT_NEAR(21, event->getHistoricalRawPointerCoords(1, 0)->getAxisValue(AMOTION_EVENT_AXIS_Y),
                EPSILON);
    ASSERT_NEAR(111, event->getHistoricalRawPointerCoords(0, 1)->getAxisValue(AMOTION_EVENT_AXIS_Y),
                EPSILON);
    ASSERT_NEAR(121, event->getHistoricalRawPointerCoords(1, 1)->getAxisValue(AMOTION_EVENT_AXIS_Y),
                EPSILON);
    ASSERT_NEAR(211, event->getRawPointerCoords(0)->getAxisValue(AMOTION_EVENT_AXIS_Y), EPSILON);
    ASSERT_NEAR(221, event->getRawPointerCoords(1)->getAxisValue(AMOTION_EVENT_AXIS_Y), EPSILON);

    ASSERT_NEAR(RAW_Y_OFFSET + 11 * RAW_Y_SCALE,
                event->getHistoricalRawAxisValue(AMOTION_EVENT_AXIS_Y, 0, 0), EPSILON);
    ASSERT_NEAR(RAW_Y_OFFSET + 21 * RAW_Y_SCALE,
                event->getHistoricalRawAxisValue(AMOTION_EVENT_AXIS_Y, 1, 0), EPSILON);
    ASSERT_NEAR(RAW_Y_OFFSET + 111 * RAW_Y_SCALE,
                event->getHistoricalRawAxisValue(AMOTION_EVENT_AXIS_Y, 0, 1), EPSILON);
    ASSERT_NEAR(RAW_Y_OFFSET + 121 * RAW_Y_SCALE,
                event->getHistoricalRawAxisValue(AMOTION_EVENT_AXIS_Y, 1, 1), EPSILON);
    ASSERT_NEAR(RAW_Y_OFFSET + 211 * RAW_Y_SCALE, event->getRawAxisValue(AMOTION_EVENT_AXIS_Y, 0),
                EPSILON);
    ASSERT_NEAR(RAW_Y_OFFSET + 221 * RAW_Y_SCALE, event->getRawAxisValue(AMOTION_EVENT_AXIS_Y, 1),
                EPSILON);

    ASSERT_NEAR(RAW_X_OFFSET + 10 * RAW_X_SCALE, event->getHistoricalRawX(0, 0), EPSILON);
    ASSERT_NEAR(RAW_X_OFFSET + 20 * RAW_X_SCALE, event->getHistoricalRawX(1, 0), EPSILON);
    ASSERT_NEAR(RAW_X_OFFSET + 110 * RAW_X_SCALE, event->getHistoricalRawX(0, 1), EPSILON);
    ASSERT_NEAR(RAW_X_OFFSET + 120 * RAW_X_SCALE, event->getHistoricalRawX(1, 1), EPSILON);
    ASSERT_NEAR(RAW_X_OFFSET + 210 * RAW_X_SCALE, event->getRawX(0), EPSILON);
    ASSERT_NEAR(RAW_X_OFFSET + 220 * RAW_X_SCALE, event->getRawX(1), EPSILON);

    ASSERT_NEAR(RAW_Y_OFFSET + 11 * RAW_Y_SCALE, event->getHistoricalRawY(0, 0), EPSILON);
    ASSERT_NEAR(RAW_Y_OFFSET + 21 * RAW_Y_SCALE, event->getHistoricalRawY(1, 0), EPSILON);
    ASSERT_NEAR(RAW_Y_OFFSET + 111 * RAW_Y_SCALE, event->getHistoricalRawY(0, 1), EPSILON);
    ASSERT_NEAR(RAW_Y_OFFSET + 121 * RAW_Y_SCALE, event->getHistoricalRawY(1, 1), EPSILON);
    ASSERT_NEAR(RAW_Y_OFFSET + 211 * RAW_Y_SCALE, event->getRawY(0), EPSILON);
    ASSERT_NEAR(RAW_Y_OFFSET + 221 * RAW_Y_SCALE, event->getRawY(1), EPSILON);

    ASSERT_NEAR(X_OFFSET + 10 * X_SCALE, event->getHistoricalX(0, 0), EPSILON);
    ASSERT_NEAR(X_OFFSET + 20 * X_SCALE, event->getHistoricalX(1, 0), EPSILON);
    ASSERT_NEAR(X_OFFSET + 110 * X_SCALE, event->getHistoricalX(0, 1), EPSILON);
    ASSERT_NEAR(X_OFFSET + 120 * X_SCALE, event->getHistoricalX(1, 1), EPSILON);
    ASSERT_NEAR(X_OFFSET + 210 * X_SCALE, event->getX(0), EPSILON);
    ASSERT_NEAR(X_OFFSET + 220 * X_SCALE, event->getX(1), EPSILON);

    ASSERT_NEAR(Y_OFFSET + 11 * Y_SCALE, event->getHistoricalY(0, 0), EPSILON);
    ASSERT_NEAR(Y_OFFSET + 21 * Y_SCALE, event->getHistoricalY(1, 0), EPSILON);
    ASSERT_NEAR(Y_OFFSET + 111 * Y_SCALE, event->getHistoricalY(0, 1), EPSILON);
    ASSERT_NEAR(Y_OFFSET + 121 * Y_SCALE, event->getHistoricalY(1, 1), EPSILON);
    ASSERT_NEAR(Y_OFFSET + 211 * Y_SCALE, event->getY(0), EPSILON);
    ASSERT_NEAR(Y_OFFSET + 221 * Y_SCALE, event->getY(1), EPSILON);

    ASSERT_EQ(12, event->getHistoricalPressure(0, 0));
    ASSERT_EQ(22, event->getHistoricalPressure(1, 0));
    ASSERT_EQ(112, event->getHistoricalPressure(0, 1));
    ASSERT_EQ(122, event->getHistoricalPressure(1, 1));
    ASSERT_EQ(212, event->getPressure(0));
    ASSERT_EQ(222, event->getPressure(1));

    ASSERT_EQ(13, event->getHistoricalSize(0, 0));
    ASSERT_EQ(23, event->getHistoricalSize(1, 0));
    ASSERT_EQ(113, event->getHistoricalSize(0, 1));
    ASSERT_EQ(123, event->getHistoricalSize(1, 1));
    ASSERT_EQ(213, event->getSize(0));
    ASSERT_EQ(223, event->getSize(1));

    ASSERT_EQ(14, event->getHistoricalTouchMajor(0, 0));
    ASSERT_EQ(24, event->getHistoricalTouchMajor(1, 0));
    ASSERT_EQ(114, event->getHistoricalTouchMajor(0, 1));
    ASSERT_EQ(124, event->getHistoricalTouchMajor(1, 1));
    ASSERT_EQ(214, event->getTouchMajor(0));
    ASSERT_EQ(224, event->getTouchMajor(1));

    ASSERT_EQ(15, event->getHistoricalTouchMinor(0, 0));
    ASSERT_EQ(25, event->getHistoricalTouchMinor(1, 0));
    ASSERT_EQ(115, event->getHistoricalTouchMinor(0, 1));
    ASSERT_EQ(125, event->getHistoricalTouchMinor(1, 1));
    ASSERT_EQ(215, event->getTouchMinor(0));
    ASSERT_EQ(225, event->getTouchMinor(1));

    ASSERT_EQ(16, event->getHistoricalToolMajor(0, 0));
    ASSERT_EQ(26, event->getHistoricalToolMajor(1, 0));
    ASSERT_EQ(116, event->getHistoricalToolMajor(0, 1));
    ASSERT_EQ(126, event->getHistoricalToolMajor(1, 1));
    ASSERT_EQ(216, event->getToolMajor(0));
    ASSERT_EQ(226, event->getToolMajor(1));

    ASSERT_EQ(17, event->getHistoricalToolMinor(0, 0));
    ASSERT_EQ(27, event->getHistoricalToolMinor(1, 0));
    ASSERT_EQ(117, event->getHistoricalToolMinor(0, 1));
    ASSERT_EQ(127, event->getHistoricalToolMinor(1, 1));
    ASSERT_EQ(217, event->getToolMinor(0));
    ASSERT_EQ(227, event->getToolMinor(1));

    // Calculate the orientation after scaling, keeping in mind that an orientation of 0 is "up",
    // and the positive y direction is "down".
    auto toScaledOrientation = [](float angle) {
        const float x = sinf(angle) * X_SCALE;
        const float y = -cosf(angle) * Y_SCALE;
        return atan2f(x, -y);
    };
    ASSERT_EQ(toScaledOrientation(18), event->getHistoricalOrientation(0, 0));
    ASSERT_EQ(toScaledOrientation(28), event->getHistoricalOrientation(1, 0));
    ASSERT_EQ(toScaledOrientation(118), event->getHistoricalOrientation(0, 1));
    ASSERT_EQ(toScaledOrientation(128), event->getHistoricalOrientation(1, 1));
    ASSERT_EQ(toScaledOrientation(218), event->getOrientation(0));
    ASSERT_EQ(toScaledOrientation(228), event->getOrientation(1));

    ASSERT_TRUE(event->isResampled(0, 0));
    ASSERT_FALSE(event->isResampled(1, 0));
    ASSERT_TRUE(event->isResampled(0, 1));
    ASSERT_TRUE(event->isResampled(1, 1));
    ASSERT_FALSE(event->isResampled(0, 2));
    ASSERT_FALSE(event->isResampled(1, 2));
}

TEST_F(MotionEventTest, Properties) {
    MotionEvent event;

    // Initialize, add samples and check properties.
    initializeEventWithHistory(&event);
    ASSERT_NO_FATAL_FAILURE(assertEqualsEventWithHistory(&event));

    // Set source.
    event.setSource(AINPUT_SOURCE_JOYSTICK);
    ASSERT_EQ(AINPUT_SOURCE_JOYSTICK, event.getSource());

    // Set displayId.
    constexpr ui::LogicalDisplayId newDisplayId = ui::LogicalDisplayId{2};
    event.setDisplayId(newDisplayId);
    ASSERT_EQ(newDisplayId, event.getDisplayId());

    // Set action.
    event.setAction(AMOTION_EVENT_ACTION_CANCEL);
    ASSERT_EQ(AMOTION_EVENT_ACTION_CANCEL, event.getAction());

    // Set meta state.
    event.setMetaState(AMETA_CTRL_ON);
    ASSERT_EQ(AMETA_CTRL_ON, event.getMetaState());
}

TEST_F(MotionEventTest, CopyFrom_KeepHistory) {
    MotionEvent event;
    initializeEventWithHistory(&event);

    MotionEvent copy;
    copy.copyFrom(&event, /*keepHistory=*/true);

    ASSERT_NO_FATAL_FAILURE(assertEqualsEventWithHistory(&event));
}

TEST_F(MotionEventTest, CopyFrom_DoNotKeepHistory) {
    MotionEvent event;
    initializeEventWithHistory(&event);

    MotionEvent copy;
    copy.copyFrom(&event, /*keepHistory=*/false);

    ASSERT_EQ(event.getPointerCount(), copy.getPointerCount());
    ASSERT_EQ(0U, copy.getHistorySize());

    ASSERT_EQ(event.getPointerId(0), copy.getPointerId(0));
    ASSERT_EQ(event.getPointerId(1), copy.getPointerId(1));

    ASSERT_EQ(event.getEventTime(), copy.getEventTime());

    ASSERT_EQ(event.getX(0), copy.getX(0));
}

TEST_F(MotionEventTest, CheckEventIdWithHistoryIsIncremented) {
    MotionEvent event;
    constexpr int32_t ARBITRARY_ID = 42;
    event.initialize(ARBITRARY_ID, 2, AINPUT_SOURCE_TOUCHSCREEN, DISPLAY_ID, INVALID_HMAC,
                     AMOTION_EVENT_ACTION_MOVE, 0, 0, AMOTION_EVENT_EDGE_FLAG_NONE, AMETA_NONE,
                     AMOTION_EVENT_BUTTON_PRIMARY, MotionClassification::NONE, mTransform, 0, 0,
                     AMOTION_EVENT_INVALID_CURSOR_POSITION, AMOTION_EVENT_INVALID_CURSOR_POSITION,
                     mRawTransform, ARBITRARY_DOWN_TIME, ARBITRARY_EVENT_TIME, 2,
                     mPointerProperties, mSamples[0].pointerCoords);
    ASSERT_EQ(event.getId(), ARBITRARY_ID);
    event.addSample(ARBITRARY_EVENT_TIME + 1, mSamples[1].pointerCoords, ARBITRARY_ID + 1);
    ASSERT_EQ(event.getId(), ARBITRARY_ID + 1);
    event.addSample(ARBITRARY_EVENT_TIME + 2, mSamples[2].pointerCoords, ARBITRARY_ID + 2);
    ASSERT_EQ(event.getId(), ARBITRARY_ID + 2);
}

TEST_F(MotionEventTest, SplitPointerDown) {
    MotionEvent event = MotionEventBuilder(POINTER_1_DOWN, AINPUT_SOURCE_TOUCHSCREEN)
                                .downTime(ARBITRARY_DOWN_TIME)
                                .pointer(PointerBuilder(/*id=*/4, ToolType::FINGER).x(4).y(4))
                                .pointer(PointerBuilder(/*id=*/6, ToolType::FINGER).x(6).y(6))
                                .pointer(PointerBuilder(/*id=*/8, ToolType::FINGER).x(8).y(8))
                                .build();

    MotionEvent splitDown;
    std::bitset<MAX_POINTER_ID + 1> splitDownIds{};
    splitDownIds.set(6, true);
    splitDown.splitFrom(event, splitDownIds, /*eventId=*/42);
    ASSERT_EQ(splitDown.getAction(), AMOTION_EVENT_ACTION_DOWN);
    ASSERT_EQ(splitDown.getPointerCount(), 1u);
    ASSERT_EQ(splitDown.getPointerId(0), 6);
    ASSERT_EQ(splitDown.getX(0), 6);
    ASSERT_EQ(splitDown.getY(0), 6);

    MotionEvent splitPointerDown;
    std::bitset<MAX_POINTER_ID + 1> splitPointerDownIds{};
    splitPointerDownIds.set(6, true);
    splitPointerDownIds.set(8, true);
    splitPointerDown.splitFrom(event, splitPointerDownIds, /*eventId=*/42);
    ASSERT_EQ(splitPointerDown.getAction(), POINTER_0_DOWN);
    ASSERT_EQ(splitPointerDown.getPointerCount(), 2u);
    ASSERT_EQ(splitPointerDown.getPointerId(0), 6);
    ASSERT_EQ(splitPointerDown.getX(0), 6);
    ASSERT_EQ(splitPointerDown.getY(0), 6);
    ASSERT_EQ(splitPointerDown.getPointerId(1), 8);
    ASSERT_EQ(splitPointerDown.getX(1), 8);
    ASSERT_EQ(splitPointerDown.getY(1), 8);

    MotionEvent splitMove;
    std::bitset<MAX_POINTER_ID + 1> splitMoveIds{};
    splitMoveIds.set(4, true);
    splitMove.splitFrom(event, splitMoveIds, /*eventId=*/43);
    ASSERT_EQ(splitMove.getAction(), AMOTION_EVENT_ACTION_MOVE);
    ASSERT_EQ(splitMove.getPointerCount(), 1u);
    ASSERT_EQ(splitMove.getPointerId(0), 4);
    ASSERT_EQ(splitMove.getX(0), 4);
    ASSERT_EQ(splitMove.getY(0), 4);
}

TEST_F(MotionEventTest, SplitPointerUp) {
    MotionEvent event = MotionEventBuilder(POINTER_0_UP, AINPUT_SOURCE_TOUCHSCREEN)
                                .downTime(ARBITRARY_DOWN_TIME)
                                .pointer(PointerBuilder(/*id=*/4, ToolType::FINGER).x(4).y(4))
                                .pointer(PointerBuilder(/*id=*/6, ToolType::FINGER).x(6).y(6))
                                .pointer(PointerBuilder(/*id=*/8, ToolType::FINGER).x(8).y(8))
                                .build();

    MotionEvent splitUp;
    std::bitset<MAX_POINTER_ID + 1> splitUpIds{};
    splitUpIds.set(4, true);
    splitUp.splitFrom(event, splitUpIds, /*eventId=*/42);
    ASSERT_EQ(splitUp.getAction(), AMOTION_EVENT_ACTION_UP);
    ASSERT_EQ(splitUp.getPointerCount(), 1u);
    ASSERT_EQ(splitUp.getPointerId(0), 4);
    ASSERT_EQ(splitUp.getX(0), 4);
    ASSERT_EQ(splitUp.getY(0), 4);

    MotionEvent splitPointerUp;
    std::bitset<MAX_POINTER_ID + 1> splitPointerUpIds{};
    splitPointerUpIds.set(4, true);
    splitPointerUpIds.set(8, true);
    splitPointerUp.splitFrom(event, splitPointerUpIds, /*eventId=*/42);
    ASSERT_EQ(splitPointerUp.getAction(), POINTER_0_UP);
    ASSERT_EQ(splitPointerUp.getPointerCount(), 2u);
    ASSERT_EQ(splitPointerUp.getPointerId(0), 4);
    ASSERT_EQ(splitPointerUp.getX(0), 4);
    ASSERT_EQ(splitPointerUp.getY(0), 4);
    ASSERT_EQ(splitPointerUp.getPointerId(1), 8);
    ASSERT_EQ(splitPointerUp.getX(1), 8);
    ASSERT_EQ(splitPointerUp.getY(1), 8);

    MotionEvent splitMove;
    std::bitset<MAX_POINTER_ID + 1> splitMoveIds{};
    splitMoveIds.set(6, true);
    splitMoveIds.set(8, true);
    splitMove.splitFrom(event, splitMoveIds, /*eventId=*/43);
    ASSERT_EQ(splitMove.getAction(), AMOTION_EVENT_ACTION_MOVE);
    ASSERT_EQ(splitMove.getPointerCount(), 2u);
    ASSERT_EQ(splitMove.getPointerId(0), 6);
    ASSERT_EQ(splitMove.getX(0), 6);
    ASSERT_EQ(splitMove.getY(0), 6);
    ASSERT_EQ(splitMove.getPointerId(1), 8);
    ASSERT_EQ(splitMove.getX(1), 8);
    ASSERT_EQ(splitMove.getY(1), 8);
}

TEST_F(MotionEventTest, SplitPointerUpCancel) {
    MotionEvent event = MotionEventBuilder(POINTER_1_UP, AINPUT_SOURCE_TOUCHSCREEN)
                                .downTime(ARBITRARY_DOWN_TIME)
                                .pointer(PointerBuilder(/*id=*/4, ToolType::FINGER).x(4).y(4))
                                .pointer(PointerBuilder(/*id=*/6, ToolType::FINGER).x(6).y(6))
                                .pointer(PointerBuilder(/*id=*/8, ToolType::FINGER).x(8).y(8))
                                .addFlag(AMOTION_EVENT_FLAG_CANCELED)
                                .build();

    MotionEvent splitUp;
    std::bitset<MAX_POINTER_ID + 1> splitUpIds{};
    splitUpIds.set(6, true);
    splitUp.splitFrom(event, splitUpIds, /*eventId=*/42);
    ASSERT_EQ(splitUp.getAction(), AMOTION_EVENT_ACTION_CANCEL);
    ASSERT_EQ(splitUp.getPointerCount(), 1u);
    ASSERT_EQ(splitUp.getPointerId(0), 6);
    ASSERT_EQ(splitUp.getX(0), 6);
    ASSERT_EQ(splitUp.getY(0), 6);
}

TEST_F(MotionEventTest, SplitPointerMove) {
    MotionEvent event = MotionEventBuilder(AMOTION_EVENT_ACTION_MOVE, AINPUT_SOURCE_TOUCHSCREEN)
                                .downTime(ARBITRARY_DOWN_TIME)
                                .pointer(PointerBuilder(/*id=*/4, ToolType::FINGER).x(4).y(4))
                                .pointer(PointerBuilder(/*id=*/6, ToolType::FINGER).x(6).y(6))
                                .pointer(PointerBuilder(/*id=*/8, ToolType::FINGER).x(8).y(8))
                                .transform(ui::Transform(ui::Transform::ROT_90, 100, 100))
                                .rawTransform(ui::Transform(ui::Transform::FLIP_H, 50, 50))
                                .build();

    MotionEvent splitMove;
    std::bitset<MAX_POINTER_ID + 1> splitMoveIds{};
    splitMoveIds.set(4, true);
    splitMoveIds.set(8, true);
    splitMove.splitFrom(event, splitMoveIds, /*eventId=*/42);
    ASSERT_EQ(splitMove.getAction(), AMOTION_EVENT_ACTION_MOVE);
    ASSERT_EQ(splitMove.getPointerCount(), 2u);
    ASSERT_EQ(splitMove.getPointerId(0), 4);
    ASSERT_EQ(splitMove.getX(0), event.getX(0));
    ASSERT_EQ(splitMove.getY(0), event.getY(0));
    ASSERT_EQ(splitMove.getRawX(0), event.getRawX(0));
    ASSERT_EQ(splitMove.getRawY(0), event.getRawY(0));
    ASSERT_EQ(splitMove.getPointerId(1), 8);
    ASSERT_EQ(splitMove.getX(1), event.getX(2));
    ASSERT_EQ(splitMove.getY(1), event.getY(2));
    ASSERT_EQ(splitMove.getRawX(1), event.getRawX(2));
    ASSERT_EQ(splitMove.getRawY(1), event.getRawY(2));
}

TEST_F(MotionEventTest, OffsetLocation) {
    MotionEvent event;
    initializeEventWithHistory(&event);
    const float xOffset = event.getRawXOffset();
    const float yOffset = event.getRawYOffset();

    event.offsetLocation(5.0f, -2.0f);

    ASSERT_EQ(xOffset + 5.0f, event.getRawXOffset());
    ASSERT_EQ(yOffset - 2.0f, event.getRawYOffset());
}

TEST_F(MotionEventTest, Scale) {
    MotionEvent event;
    initializeEventWithHistory(&event);
    const float unscaledOrientation = event.getOrientation(0);
    const float unscaledXOffset = event.getRawXOffset();
    const float unscaledYOffset = event.getRawYOffset();

    event.scale(2.0f);

    ASSERT_EQ(unscaledXOffset * 2, event.getRawXOffset());
    ASSERT_EQ(unscaledYOffset * 2, event.getRawYOffset());

    ASSERT_NEAR((RAW_X_OFFSET + 210 * RAW_X_SCALE) * 2, event.getRawX(0), EPSILON);
    ASSERT_NEAR((RAW_Y_OFFSET + 211 * RAW_Y_SCALE) * 2, event.getRawY(0), EPSILON);
    ASSERT_NEAR((X_OFFSET + 210 * X_SCALE) * 2, event.getX(0), EPSILON);
    ASSERT_NEAR((Y_OFFSET + 211 * Y_SCALE) * 2, event.getY(0), EPSILON);
    ASSERT_EQ(212, event.getPressure(0));
    ASSERT_EQ(213, event.getSize(0));
    ASSERT_EQ(214 * 2, event.getTouchMajor(0));
    ASSERT_EQ(215 * 2, event.getTouchMinor(0));
    ASSERT_EQ(216 * 2, event.getToolMajor(0));
    ASSERT_EQ(217 * 2, event.getToolMinor(0));
    ASSERT_EQ(unscaledOrientation, event.getOrientation(0));
}

TEST_F(MotionEventTest, Parcel) {
    Parcel parcel;

    MotionEvent inEvent;
    initializeEventWithHistory(&inEvent);
    MotionEvent outEvent;

    // Round trip.
    inEvent.writeToParcel(&parcel);
    parcel.setDataPosition(0);
    outEvent.readFromParcel(&parcel);

    ASSERT_NO_FATAL_FAILURE(assertEqualsEventWithHistory(&outEvent));
}

static void setRotationMatrix(std::array<float, 9>& matrix, float angle) {
    float sin = sinf(angle);
    float cos = cosf(angle);
    matrix[0] = cos;
    matrix[1] = -sin;
    matrix[2] = 0;
    matrix[3] = sin;
    matrix[4] = cos;
    matrix[5] = 0;
    matrix[6] = 0;
    matrix[7] = 0;
    matrix[8] = 1.0f;
}

TEST_F(MotionEventTest, Transform) {
    // Generate some points on a circle.
    // Each point 'i' is a point on a circle of radius ROTATION centered at (3,2) at an angle
    // of ARC * i degrees clockwise relative to the Y axis.
    // The geometrical representation is irrelevant to the test, it's just easy to generate
    // and check rotation.  We set the orientation to the same angle.
    // Coordinate system: down is increasing Y, right is increasing X.
    static constexpr float PI_180 = float(M_PI / 180);
    static constexpr float RADIUS = 10;
    static constexpr float ARC = 36;
    static constexpr float ROTATION = ARC * 2;

    const size_t pointerCount = 11;
    PointerProperties pointerProperties[pointerCount];
    PointerCoords pointerCoords[pointerCount];
    for (size_t i = 0; i < pointerCount; i++) {
        float angle = float(i * ARC * PI_180);
        pointerProperties[i].clear();
        pointerProperties[i].id = i;
        pointerCoords[i].clear();
        pointerCoords[i].setAxisValue(AMOTION_EVENT_AXIS_X, sinf(angle) * RADIUS + 3);
        pointerCoords[i].setAxisValue(AMOTION_EVENT_AXIS_Y, -cosf(angle) * RADIUS + 2);
        pointerCoords[i].setAxisValue(AMOTION_EVENT_AXIS_ORIENTATION, angle);
    }
    MotionEvent event;
    ui::Transform identityTransform;
    const int32_t flags = AMOTION_EVENT_PRIVATE_FLAG_SUPPORTS_ORIENTATION |
            AMOTION_EVENT_PRIVATE_FLAG_SUPPORTS_DIRECTIONAL_ORIENTATION;
    event.initialize(InputEvent::nextId(), /*deviceId=*/0, AINPUT_SOURCE_TOUCHSCREEN, DISPLAY_ID,
                     INVALID_HMAC, AMOTION_EVENT_ACTION_MOVE, /*actionButton=*/0, flags,
                     AMOTION_EVENT_EDGE_FLAG_NONE, AMETA_NONE, /*buttonState=*/0,
                     MotionClassification::NONE, identityTransform, /*xPrecision=*/0,
                     /*yPrecision=*/0, /*xCursorPosition=*/3 + RADIUS, /*yCursorPosition=*/2,
                     identityTransform, /*downTime=*/0, /*eventTime=*/0, pointerCount,
                     pointerProperties, pointerCoords);
    float originalRawX = 0 + 3;
    float originalRawY = -RADIUS + 2;

    // Check original raw X and Y assumption.
    ASSERT_NEAR(originalRawX, event.getRawX(0), 0.001);
    ASSERT_NEAR(originalRawY, event.getRawY(0), 0.001);

    // Now translate the motion event so the circle's origin is at (0,0).
    event.offsetLocation(-3, -2);

    // Offsetting the location should preserve the raw X and Y of the first point.
    ASSERT_NEAR(originalRawX, event.getRawX(0), 0.001);
    ASSERT_NEAR(originalRawY, event.getRawY(0), 0.001);

    // Apply a rotation about the origin by ROTATION degrees clockwise.
    std::array<float, 9> matrix;
    setRotationMatrix(matrix, ROTATION * PI_180);
    event.transform(matrix);

    // Check the points.
    for (size_t i = 0; i < pointerCount; i++) {
        float angle = float((i * ARC + ROTATION) * PI_180);
        ASSERT_NEAR(sinf(angle) * RADIUS, event.getX(i), 0.001);
        ASSERT_NEAR(-cosf(angle) * RADIUS, event.getY(i), 0.001);
        ASSERT_NEAR(tanf(angle), tanf(event.getOrientation(i)), 0.1);
    }

    // Check cursor positions. The original cursor position is at (3 + RADIUS, 2), where the center
    // of the circle is (3, 2), so the cursor position is to the right of the center of the circle.
    // The choice of triangular functions in this test defines the angle of rotation clockwise
    // relative to the y-axis. Therefore the cursor position's angle is 90 degrees. Here we swap the
    // triangular function so that we don't have to add the 90 degrees.
    ASSERT_NEAR(cosf(PI_180 * ROTATION) * RADIUS, event.getXCursorPosition(), 0.001);
    ASSERT_NEAR(sinf(PI_180 * ROTATION) * RADIUS, event.getYCursorPosition(), 0.001);

    // Applying the transformation should preserve the raw X and Y of the first point.
    ASSERT_NEAR(originalRawX, event.getRawX(0), 0.001);
    ASSERT_NEAR(originalRawY, event.getRawY(0), 0.001);
}

MotionEvent createMotionEvent(int32_t source, uint32_t action, float x, float y, float dx, float dy,
                              const ui::Transform& transform, const ui::Transform& rawTransform) {
    std::vector<PointerProperties> pointerProperties;
    pointerProperties.push_back(PointerProperties{/*id=*/0, ToolType::FINGER});
    std::vector<PointerCoords> pointerCoords;
    pointerCoords.emplace_back().clear();
    pointerCoords.back().setAxisValue(AMOTION_EVENT_AXIS_X, x);
    pointerCoords.back().setAxisValue(AMOTION_EVENT_AXIS_Y, y);
    pointerCoords.back().setAxisValue(AMOTION_EVENT_AXIS_RELATIVE_X, dx);
    pointerCoords.back().setAxisValue(AMOTION_EVENT_AXIS_RELATIVE_Y, dy);
    nsecs_t eventTime = systemTime(SYSTEM_TIME_MONOTONIC);
    MotionEvent event;
    event.initialize(InputEvent::nextId(), /*deviceId=*/1, source, ui::LogicalDisplayId::DEFAULT,
                     INVALID_HMAC, action, /*actionButton=*/0, /*flags=*/0, /*edgeFlags=*/0,
                     AMETA_NONE, /*buttonState=*/0, MotionClassification::NONE, transform,
                     /*xPrecision=*/0, /*yPrecision=*/0, AMOTION_EVENT_INVALID_CURSOR_POSITION,
                     AMOTION_EVENT_INVALID_CURSOR_POSITION, rawTransform, eventTime, eventTime,
                     pointerCoords.size(), pointerProperties.data(), pointerCoords.data());
    return event;
}

MotionEvent createTouchDownEvent(float x, float y, float dx, float dy,
                                 const ui::Transform& transform,
                                 const ui::Transform& rawTransform) {
    return createMotionEvent(AINPUT_SOURCE_TOUCHSCREEN, AMOTION_EVENT_ACTION_DOWN, x, y, dx, dy,
                             transform, rawTransform);
}

TEST_F(MotionEventTest, ApplyTransform) {
    // Create a rotate-90 transform with an offset (like a window which isn't fullscreen).
    ui::Transform identity;
    ui::Transform transform(ui::Transform::ROT_90, 800, 400);
    transform.set(transform.tx() + 20, transform.ty() + 40);
    ui::Transform rawTransform(ui::Transform::ROT_90, 800, 400);
    MotionEvent event = createTouchDownEvent(60, 100, 42, 96, transform, rawTransform);
    ASSERT_EQ(700, event.getRawX(0));
    ASSERT_EQ(60, event.getRawY(0));
    ASSERT_NE(event.getRawX(0), event.getX(0));
    ASSERT_NE(event.getRawY(0), event.getY(0));
    // Relative values should be rotated but not translated.
    ASSERT_EQ(-96, event.getAxisValue(AMOTION_EVENT_AXIS_RELATIVE_X, 0));
    ASSERT_EQ(42, event.getAxisValue(AMOTION_EVENT_AXIS_RELATIVE_Y, 0));

    MotionEvent changedEvent = createTouchDownEvent(60, 100, 42, 96, identity, identity);
    const std::array<float, 9> rowMajor{transform[0][0], transform[1][0], transform[2][0],
                                        transform[0][1], transform[1][1], transform[2][1],
                                        transform[0][2], transform[1][2], transform[2][2]};
    changedEvent.applyTransform(rowMajor);

    // transformContent effectively rotates the raw coordinates, so those should now include
    // both rotation AND offset.
    ASSERT_EQ(720, changedEvent.getRawX(0));
    ASSERT_EQ(100, changedEvent.getRawY(0));
    // Relative values should be rotated but not translated.
    ASSERT_EQ(-96, event.getAxisValue(AMOTION_EVENT_AXIS_RELATIVE_X, 0));
    ASSERT_EQ(42, event.getAxisValue(AMOTION_EVENT_AXIS_RELATIVE_Y, 0));

    // The transformed output should be the same then.
    ASSERT_NEAR(event.getX(0), changedEvent.getX(0), 0.001);
    ASSERT_NEAR(event.getY(0), changedEvent.getY(0), 0.001);
    ASSERT_NEAR(event.getAxisValue(AMOTION_EVENT_AXIS_RELATIVE_X, 0),
                changedEvent.getAxisValue(AMOTION_EVENT_AXIS_RELATIVE_X, 0), 0.001);
    ASSERT_NEAR(event.getAxisValue(AMOTION_EVENT_AXIS_RELATIVE_Y, 0),
                changedEvent.getAxisValue(AMOTION_EVENT_AXIS_RELATIVE_Y, 0), 0.001);
}

TEST_F(MotionEventTest, JoystickAndTouchpadAreNotTransformed) {
    constexpr static std::array kNonTransformedSources =
            {std::pair(AINPUT_SOURCE_TOUCHPAD, AMOTION_EVENT_ACTION_DOWN),
             std::pair(AINPUT_SOURCE_JOYSTICK, AMOTION_EVENT_ACTION_MOVE),
             std::pair(AINPUT_SOURCE_MOUSE_RELATIVE, AMOTION_EVENT_ACTION_MOVE)};
    // Create a rotate-90 transform with an offset (like a window which isn't fullscreen).
    ui::Transform transform(ui::Transform::ROT_90, 800, 400);
    transform.set(transform.tx() + 20, transform.ty() + 40);

    for (const auto& [source, action] : kNonTransformedSources) {
        const MotionEvent event =
                createMotionEvent(source, action, 60, 100, 0, 0, transform, transform);

        // These events should not be transformed in any way.
        ASSERT_EQ(60, event.getX(0));
        ASSERT_EQ(100, event.getY(0));
        ASSERT_EQ(event.getRawX(0), event.getX(0));
        ASSERT_EQ(event.getRawY(0), event.getY(0));
    }
}

TEST_F(MotionEventTest, NonPointerSourcesAreNotTranslated) {
    constexpr static std::array kNonPointerSources = {std::pair(AINPUT_SOURCE_TRACKBALL,
                                                                AMOTION_EVENT_ACTION_DOWN),
                                                      std::pair(AINPUT_SOURCE_TOUCH_NAVIGATION,
                                                                AMOTION_EVENT_ACTION_MOVE)};
    // Create a rotate-90 transform with an offset (like a window which isn't fullscreen).
    ui::Transform transform(ui::Transform::ROT_90, 800, 400);
    transform.set(transform.tx() + 20, transform.ty() + 40);

    for (const auto& [source, action] : kNonPointerSources) {
        const MotionEvent event =
                createMotionEvent(source, action, 60, 100, 42, 96, transform, transform);

        // Since this event comes from a non-pointer source, it should include rotation but not
        // translation/offset.
        ASSERT_EQ(-100, event.getX(0));
        ASSERT_EQ(60, event.getY(0));
        ASSERT_EQ(event.getRawX(0), event.getX(0));
        ASSERT_EQ(event.getRawY(0), event.getY(0));
    }
}

TEST_F(MotionEventTest, AxesAreCorrectlyTransformed) {
    const ui::Transform identity;
    ui::Transform transform;
    transform.set({1.1, -2.2, 3.3, -4.4, 5.5, -6.6, 0, 0, 1});
    ui::Transform rawTransform;
    rawTransform.set({-6.6, 5.5, -4.4, 3.3, -2.2, 1.1, 0, 0, 1});
    auto transformWithoutTranslation = [](const ui::Transform& t, float x, float y) {
        auto newPoint = t.transform(x, y);
        auto newOrigin = t.transform(0, 0);
        return newPoint - newOrigin;
    };

    const MotionEvent event = createTouchDownEvent(60, 100, 42, 96, transform, rawTransform);

    // The x and y axes should have the window transform applied.
    const auto newPoint = transform.transform(60, 100);
    ASSERT_NEAR(newPoint.x, event.getX(0), EPSILON);
    ASSERT_NEAR(newPoint.y, event.getY(0), EPSILON);

    // The raw values should have the display transform applied.
    const auto raw = rawTransform.transform(60, 100);
    ASSERT_NEAR(raw.x, event.getRawX(0), EPSILON);
    ASSERT_NEAR(raw.y, event.getRawY(0), EPSILON);

    // Relative values should have the window transform applied without any translation.
    const auto rel = transformWithoutTranslation(transform, 42, 96);
    ASSERT_NEAR(rel.x, event.getAxisValue(AMOTION_EVENT_AXIS_RELATIVE_X, 0), EPSILON);
    ASSERT_NEAR(rel.y, event.getAxisValue(AMOTION_EVENT_AXIS_RELATIVE_Y, 0), EPSILON);
}

TEST_F(MotionEventTest, Initialize_SetsClassification) {
    std::array<MotionClassification, 3> classifications = {
            MotionClassification::NONE,
            MotionClassification::AMBIGUOUS_GESTURE,
            MotionClassification::DEEP_PRESS,
    };

    MotionEvent event;
    constexpr size_t pointerCount = 1;
    PointerProperties pointerProperties[pointerCount];
    PointerCoords pointerCoords[pointerCount];
    for (size_t i = 0; i < pointerCount; i++) {
        pointerProperties[i].clear();
        pointerProperties[i].id = i;
        pointerCoords[i].clear();
    }

    ui::Transform identityTransform;
    for (MotionClassification classification : classifications) {
        event.initialize(InputEvent::nextId(), /*deviceId=*/0, AINPUT_SOURCE_TOUCHSCREEN,
                         DISPLAY_ID, INVALID_HMAC, AMOTION_EVENT_ACTION_DOWN, 0, 0,
                         AMOTION_EVENT_EDGE_FLAG_NONE, AMETA_NONE, 0, classification,
                         identityTransform, 0, 0, AMOTION_EVENT_INVALID_CURSOR_POSITION,
                         AMOTION_EVENT_INVALID_CURSOR_POSITION, identityTransform, /*downTime=*/0,
                         /*eventTime=*/0, pointerCount, pointerProperties, pointerCoords);
        ASSERT_EQ(classification, event.getClassification());
    }
}

TEST_F(MotionEventTest, Initialize_SetsCursorPosition) {
    MotionEvent event;
    constexpr size_t pointerCount = 1;
    PointerProperties pointerProperties[pointerCount];
    PointerCoords pointerCoords[pointerCount];
    for (size_t i = 0; i < pointerCount; i++) {
        pointerProperties[i].clear();
        pointerProperties[i].id = i;
        pointerCoords[i].clear();
    }

    ui::Transform identityTransform;
    event.initialize(InputEvent::nextId(), /*deviceId=*/0, AINPUT_SOURCE_MOUSE, DISPLAY_ID,
                     INVALID_HMAC, AMOTION_EVENT_ACTION_DOWN, 0, 0, AMOTION_EVENT_EDGE_FLAG_NONE,
                     AMETA_NONE, 0, MotionClassification::NONE, identityTransform, 0, 0,
                     /*xCursorPosition=*/280, /*yCursorPosition=*/540, identityTransform,
                     /*downTime=*/0, /*eventTime=*/0, pointerCount, pointerProperties,
                     pointerCoords);
    event.offsetLocation(20, 60);
    ASSERT_EQ(280, event.getRawXCursorPosition());
    ASSERT_EQ(540, event.getRawYCursorPosition());
    ASSERT_EQ(300, event.getXCursorPosition());
    ASSERT_EQ(600, event.getYCursorPosition());
}

TEST_F(MotionEventTest, SetCursorPosition) {
    MotionEvent event;
    initializeEventWithHistory(&event);
    event.setSource(AINPUT_SOURCE_MOUSE);

    event.setCursorPosition(3, 4);
    ASSERT_EQ(3, event.getXCursorPosition());
    ASSERT_EQ(4, event.getYCursorPosition());
}

TEST_F(MotionEventTest, CoordinatesAreRoundedAppropriately) {
    // These are specifically integral values, since we are testing for rounding.
    const vec2 EXPECTED{400.f, 700.f};

    // Pick a transform such that transforming the point with its inverse and bringing that
    // back to the original coordinate space results in a non-zero error amount due to the
    // nature of floating point arithmetics. This can happen when the display is scaled.
    // For example, the 'adb shell wm size' command can be used to set an override for the
    // logical display size, which could result in the display being scaled.
    constexpr float scale = 720.f / 1080.f;
    ui::Transform transform;
    transform.set(scale, 0, 0, scale);
    ASSERT_NE(EXPECTED, transform.transform(transform.inverse().transform(EXPECTED)));

    // Store the inverse-transformed values in the motion event.
    const vec2 rawCoords = transform.inverse().transform(EXPECTED);
    PointerCoords pc{};
    pc.setAxisValue(AMOTION_EVENT_AXIS_X, rawCoords.x);
    pc.setAxisValue(AMOTION_EVENT_AXIS_Y, rawCoords.y);
    PointerProperties pp{};
    MotionEvent event;
    event.initialize(InputEvent::nextId(), 2, AINPUT_SOURCE_TOUCHSCREEN, DISPLAY_ID, HMAC,
                     AMOTION_EVENT_ACTION_MOVE, 0, AMOTION_EVENT_FLAG_WINDOW_IS_OBSCURED,
                     AMOTION_EVENT_EDGE_FLAG_TOP, AMETA_ALT_ON, AMOTION_EVENT_BUTTON_PRIMARY,
                     MotionClassification::NONE, transform, 2.0f, 2.1f, rawCoords.x, rawCoords.y,
                     transform, ARBITRARY_DOWN_TIME, ARBITRARY_EVENT_TIME, 1, &pp, &pc);

    // When using the getters from the MotionEvent to obtain the coordinates, the transformed
    // values should be rounded by an appropriate amount so that they now precisely equal the
    // original coordinates.
    ASSERT_EQ(EXPECTED.x, event.getX(0));
    ASSERT_EQ(EXPECTED.y, event.getY(0));
    ASSERT_EQ(EXPECTED.x, event.getRawX(0));
    ASSERT_EQ(EXPECTED.y, event.getRawY(0));
    ASSERT_EQ(EXPECTED.x, event.getXCursorPosition());
    ASSERT_EQ(EXPECTED.y, event.getYCursorPosition());
}

TEST_F(MotionEventTest, InvalidOrientationNotRotated) {
    // This touch event does not have a value for AXIS_ORIENTATION, and the flags are implicitly
    // set to 0. The transform is set to a 90-degree rotation.
    MotionEvent event = MotionEventBuilder(AMOTION_EVENT_ACTION_MOVE, AINPUT_SOURCE_TOUCHSCREEN)
                                .downTime(ARBITRARY_DOWN_TIME)
                                .pointer(PointerBuilder(/*id=*/4, ToolType::FINGER).x(4).y(4))
                                .transform(ui::Transform(ui::Transform::ROT_90, 100, 100))
                                .rawTransform(ui::Transform(ui::Transform::FLIP_H, 50, 50))
                                .build();
    ASSERT_EQ(event.getOrientation(/*pointerIndex=*/0), 0.f);
    event.transform(asFloat9(ui::Transform(ui::Transform::ROT_90, 100, 100)));
    ASSERT_EQ(event.getOrientation(/*pointerIndex=*/0), 0.f);
    event.transform(asFloat9(ui::Transform(ui::Transform::ROT_180, 100, 100)));
    ASSERT_EQ(event.getOrientation(/*pointerIndex=*/0), 0.f);
    event.applyTransform(asFloat9(ui::Transform(ui::Transform::ROT_270, 100, 100)));
    ASSERT_EQ(event.getOrientation(/*pointerIndex=*/0), 0.f);
}

TEST_F(MotionEventTest, ValidZeroOrientationRotated) {
    // This touch events will implicitly have a value of 0 for its AXIS_ORIENTATION.
    auto builder = MotionEventBuilder(AMOTION_EVENT_ACTION_MOVE, AINPUT_SOURCE_TOUCHSCREEN)
                           .downTime(ARBITRARY_DOWN_TIME)
                           .pointer(PointerBuilder(/*id=*/4, ToolType::FINGER).x(4).y(4))
                           .transform(ui::Transform(ui::Transform::ROT_90, 100, 100))
                           .rawTransform(ui::Transform(ui::Transform::FLIP_H, 50, 50))
                           .addFlag(AMOTION_EVENT_PRIVATE_FLAG_SUPPORTS_ORIENTATION);
    MotionEvent nonDirectionalEvent = builder.build();
    MotionEvent directionalEvent =
            builder.addFlag(AMOTION_EVENT_PRIVATE_FLAG_SUPPORTS_DIRECTIONAL_ORIENTATION).build();

    // The angle is rotated by the initial transform, a 90-degree rotation.
    ASSERT_NEAR(fabs(nonDirectionalEvent.getOrientation(/*pointerIndex=*/0)), M_PI_2, EPSILON);
    ASSERT_NEAR(directionalEvent.getOrientation(/*pointerIndex=*/0), M_PI_2, EPSILON);

    nonDirectionalEvent.transform(asFloat9(ui::Transform(ui::Transform::ROT_90, 100, 100)));
    directionalEvent.transform(asFloat9(ui::Transform(ui::Transform::ROT_90, 100, 100)));
    ASSERT_NEAR(nonDirectionalEvent.getOrientation(/*pointerIndex=*/0), 0.f, EPSILON);
    ASSERT_NEAR(fabs(directionalEvent.getOrientation(/*pointerIndex=*/0)), M_PI, EPSILON);

    nonDirectionalEvent.transform(asFloat9(ui::Transform(ui::Transform::ROT_180, 100, 100)));
    directionalEvent.transform(asFloat9(ui::Transform(ui::Transform::ROT_180, 100, 100)));
    ASSERT_NEAR(nonDirectionalEvent.getOrientation(/*pointerIndex=*/0), 0.f, EPSILON);
    ASSERT_NEAR(directionalEvent.getOrientation(/*pointerIndex=*/0), 0.f, EPSILON);

    nonDirectionalEvent.applyTransform(asFloat9(ui::Transform(ui::Transform::ROT_270, 100, 100)));
    directionalEvent.applyTransform(asFloat9(ui::Transform(ui::Transform::ROT_270, 100, 100)));
    ASSERT_NEAR(fabs(nonDirectionalEvent.getOrientation(/*pointerIndex=*/0)), M_PI_2, EPSILON);
    ASSERT_NEAR(directionalEvent.getOrientation(/*pointerIndex=*/0), -M_PI_2, EPSILON);
}

TEST_F(MotionEventTest, ValidNonZeroOrientationRotated) {
    const float initial = 1.f;
    auto builder = MotionEventBuilder(AMOTION_EVENT_ACTION_MOVE, AINPUT_SOURCE_TOUCHSCREEN)
                           .downTime(ARBITRARY_DOWN_TIME)
                           .pointer(PointerBuilder(/*id=*/4, ToolType::FINGER)
                                            .x(4)
                                            .y(4)
                                            .axis(AMOTION_EVENT_AXIS_ORIENTATION, initial))
                           .transform(ui::Transform(ui::Transform::ROT_90, 100, 100))
                           .rawTransform(ui::Transform(ui::Transform::FLIP_H, 50, 50))
                           .addFlag(AMOTION_EVENT_PRIVATE_FLAG_SUPPORTS_ORIENTATION);

    MotionEvent nonDirectionalEvent = builder.build();
    MotionEvent directionalEvent =
            builder.addFlag(AMOTION_EVENT_PRIVATE_FLAG_SUPPORTS_DIRECTIONAL_ORIENTATION).build();

    // The angle is rotated by the initial transform, a 90-degree rotation.
    ASSERT_NEAR(nonDirectionalEvent.getOrientation(/*pointerIndex=*/0), initial - M_PI_2, EPSILON);
    ASSERT_NEAR(directionalEvent.getOrientation(/*pointerIndex=*/0), initial + M_PI_2, EPSILON);

    nonDirectionalEvent.transform(asFloat9(ui::Transform(ui::Transform::ROT_90, 100, 100)));
    directionalEvent.transform(asFloat9(ui::Transform(ui::Transform::ROT_90, 100, 100)));
    ASSERT_NEAR(nonDirectionalEvent.getOrientation(/*pointerIndex=*/0), initial, EPSILON);
    ASSERT_NEAR(directionalEvent.getOrientation(/*pointerIndex=*/0), initial - M_PI, EPSILON);

    nonDirectionalEvent.transform(asFloat9(ui::Transform(ui::Transform::ROT_180, 100, 100)));
    directionalEvent.transform(asFloat9(ui::Transform(ui::Transform::ROT_180, 100, 100)));
    ASSERT_NEAR(nonDirectionalEvent.getOrientation(/*pointerIndex=*/0), initial, EPSILON);
    ASSERT_NEAR(directionalEvent.getOrientation(/*pointerIndex=*/0), initial, EPSILON);

    nonDirectionalEvent.applyTransform(asFloat9(ui::Transform(ui::Transform::ROT_270, 100, 100)));
    directionalEvent.applyTransform(asFloat9(ui::Transform(ui::Transform::ROT_270, 100, 100)));
    ASSERT_NEAR(nonDirectionalEvent.getOrientation(/*pointerIndex=*/0), initial - M_PI_2, EPSILON);
    ASSERT_NEAR(directionalEvent.getOrientation(/*pointerIndex=*/0), initial - M_PI_2, EPSILON);
}

} // namespace android
