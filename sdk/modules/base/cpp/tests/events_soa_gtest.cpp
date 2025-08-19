#include "metavision/sdk/base/events/events_soa.h"
#include <gtest/gtest.h>
#include <vector>
#include <random>

using namespace Metavision;

TEST(EventsSoATest, InitiallyEmpty) {
    EventsSoA v;
    EXPECT_TRUE(v.empty());
    EXPECT_EQ(v.size(), 0u);
}

TEST(EventsSoATest, PushBackAndAccess) {
    EventsSoA v;
    EventType val{1, 2, -3, 4};
    v.push_back(val);

    EXPECT_FALSE(v.empty());
    EXPECT_EQ(v.size(), 1u);

    auto got = v[0];
    EXPECT_EQ(got.x, 1);
    EXPECT_EQ(got.y, 2);
    EXPECT_EQ(got.p, -3);
    EXPECT_EQ(got.t, 4);
}

TEST(EventsSoATest, EmplaceBack) {
    EventsSoA v;
    v.emplace_back(10, 20, -30, 40LL);

    ASSERT_EQ(v.size(), 1u);
    auto got = v[0];
    EXPECT_EQ(got.x, 10);
    EXPECT_EQ(got.y, 20);
    EXPECT_EQ(got.p, -30);
    EXPECT_EQ(got.t, 40LL);
}

TEST(EventsSoATest, ModifyThroughReference) {
    EventsSoA v;
    v.emplace_back(1, 2, 3, 4);
    v[0].x = 100;
    v[0].y = 200;
    v[0].p = -300;
    v[0].t = 4000;

    auto got = v[0];
    EXPECT_EQ(got.x, 100);
    EXPECT_EQ(got.y, 200);
    EXPECT_EQ(got.p, -300);
    EXPECT_EQ(got.t, 4000);
}

TEST(EventsSoATest, AtThrowsOnOutOfRange) {
    EventsSoA v;
    EXPECT_THROW(v.at(0), std::out_of_range);
}

TEST(EventsSoATest, PopBack) {
    EventsSoA v;
    v.emplace_back(1, 2, 3, 4);
    v.emplace_back(5, 6, 7, 8);
    v.pop_back();

    EXPECT_EQ(v.size(), 1u);
    auto got = v[0];
    EXPECT_EQ(got.x, 1);
    EXPECT_EQ(got.y, 2);
}

TEST(EventsSoATest, Iteration) {
    EventsSoA v;
    v.emplace_back(1, 2, 3, 4);
    v.emplace_back(5, 6, 7, 8);

    std::vector<EventType> values;
    for (const auto &elem : v) {
        values.push_back(elem);
    }

    ASSERT_EQ(values.size(), 2u);
    EXPECT_EQ(values[0].x, 1);
    EXPECT_EQ(values[1].y, 6);
}

TEST(EventsSoATest, ConstIteration) {
    EventsSoA v;
    v.emplace_back(10, 20, 30, 40);
    v.emplace_back(50, 60, 70, 80);

    const EventsSoA &cv = v;
    std::vector<EventType> values;
    for (auto elem : cv) {
        values.push_back(elem);
    }

    ASSERT_EQ(values.size(), 2u);
    EXPECT_EQ(values[0].t, 40);
    EXPECT_EQ(values[1].x, 50);
}

TEST(EventsSoATest, ResizeAndReserve) {
    EventsSoA v;
    v.resize(5);
    EXPECT_EQ(v.size(), 5u);

    v.reserve(10);
    EXPECT_GE(v.size(), 5u); // reserve shouldn't shrink
}

TEST(EventsSoATest, InsertFrontBackMatchesStdVector) {
    using V = EventType;

    EventsSoA soa;
    std::vector<V> aos;

    // Fill with initial data
    for (int i = 0; i < 5; ++i) {
        V val{static_cast<uint16_t>(i),
              static_cast<uint16_t>(i + 10),
              static_cast<int16_t>(i - 5),
              i * 100LL};
        soa.push_back(val);
        aos.push_back(val);
    }

    // --- Test front() and back() initially ---
    ASSERT_EQ(soa.front().x, aos.front().x);
    ASSERT_EQ(soa.back().x, aos.back().x);
    ASSERT_EQ(soa.front().t, aos.front().t);
    ASSERT_EQ(soa.back().t, aos.back().t);

    // --- Test insert at beginning ---
    V front_val{42, 43, -44, 4500};
    soa.insert(soa.begin(), front_val);
    aos.insert(aos.begin(), front_val);

    ASSERT_EQ(soa.front().x, aos.front().x);
    ASSERT_EQ(soa.front().y, aos.front().y);
    ASSERT_EQ(soa.front().p, aos.front().p);
    ASSERT_EQ(soa.front().t, aos.front().t);

    // --- Test insert in the middle ---
    V mid_val{100, 101, -102, 10300};
    size_t mid_pos = soa.size() / 2;
    soa.insert(soa.begin() + mid_pos, mid_val);
    aos.insert(aos.begin() + mid_pos, mid_val);

    ASSERT_EQ(soa[mid_pos].x, aos[mid_pos].x);
    ASSERT_EQ(soa[mid_pos].y, aos[mid_pos].y);
    ASSERT_EQ(soa[mid_pos].p, aos[mid_pos].p);
    ASSERT_EQ(soa[mid_pos].t, aos[mid_pos].t);

    // --- Test insert at the end ---
    V end_val{200, 201, -202, 20300};
    soa.insert(soa.end(), end_val);
    aos.insert(aos.end(), end_val);

    ASSERT_EQ(soa.back().x, aos.back().x);
    ASSERT_EQ(soa.back().y, aos.back().y);
    ASSERT_EQ(soa.back().p, aos.back().p);
    ASSERT_EQ(soa.back().t, aos.back().t);

    // --- Full container comparison ---
    ASSERT_EQ(soa.size(), aos.size());
    for (size_t i = 0; i < soa.size(); ++i) {
        auto s = soa[i];
        auto a = aos[i];
        EXPECT_EQ(s.x, a.x) << "Mismatch at index " << i;
        EXPECT_EQ(s.y, a.y) << "Mismatch at index " << i;
        EXPECT_EQ(s.p, a.p) << "Mismatch at index " << i;
        EXPECT_EQ(s.t, a.t) << "Mismatch at index " << i;
    }
}

TEST(EventsSoATest, RandomizedPushPopMatchesStdVector) {
    using V = EventType;

    EventsSoA soa;
    std::vector<V> aos; // Array-of-structs baseline

    std::mt19937 rng(42); // deterministic seed
    std::uniform_int_distribution<int> opdist(0, 1); // 0 = push, 1 = pop
    std::uniform_int_distribution<int> uint16_t_dist(0, 65535);
    std::uniform_int_distribution<int> int16_t_dist(-32768, 32767);
    std::uniform_int_distribution<int64_t> longlong_dist(-1000000LL, 1000000LL);

    const int operations = 1000;
    for (int i = 0; i < operations; ++i) {
        int op = opdist(rng);

        if (op == 0 || aos.empty()) {
            // Push
            V val;
            val.x = static_cast<uint16_t>(uint16_t_dist(rng));
            val.y = static_cast<uint16_t>(uint16_t_dist(rng));
            val.p = static_cast<int16_t>(int16_t_dist(rng));
            val.t = longlong_dist(rng);

            soa.push_back(val);
            aos.push_back(val);
        }
        else {
            // Pop
            soa.pop_back();
            aos.pop_back();
        }

        // Check sizes match
        ASSERT_EQ(soa.size(), aos.size());

        // Check all elements match
        for (size_t j = 0; j < aos.size(); ++j) {
            auto s = soa[j];
            auto a = aos[j];
            ASSERT_EQ(s.x, a.x) << "Mismatch at index " << j;
            ASSERT_EQ(s.y, a.y) << "Mismatch at index " << j;
            ASSERT_EQ(s.p, a.p) << "Mismatch at index " << j;
            ASSERT_EQ(s.t, a.t) << "Mismatch at index " << j;
        }
    }
}

TEST(EventsSoA, RangeInsertMatchesStdVector) {
    EventsSoA events_soa;
    std::vector<EventType> events_std;

    // Fill initial events
    for (int i = 0; i < 10; ++i) {
        EventType e{
            static_cast<uint16_t>(i),
            static_cast<uint16_t>(i + 10),
            static_cast<int16_t>(i + 100),
            static_cast<int64_t>(i + 1000)
        };
        events_soa.push_back(e);
        events_std.push_back(e);
    }

    // Prepare a range to insert
    std::vector<EventType> range;
    for (int i = 0; i < 5; ++i) {
        range.push_back(EventType{
            static_cast<uint16_t>(100 + i),
            static_cast<uint16_t>(200 + i),
            static_cast<int16_t>(300 + i),
            static_cast<int64_t>(400 + i)
        });
    }

    // Insert in the middle
    auto insert_pos_soa = events_soa.begin() + 5;
    auto insert_pos_std = events_std.begin() + 5;
    events_soa.insert(insert_pos_soa, range.begin(), range.end());
    events_std.insert(insert_pos_std, range.begin(), range.end());

    // Compare the full sequences
    ASSERT_EQ(events_soa.size(), events_std.size());
    for (size_t i = 0; i < events_soa.size(); ++i) {
        const auto &e_soa = events_soa[i];  // <-- use const reference
        const auto &e_std = events_std[i];
        EXPECT_EQ(e_soa.x, e_std.x);
        EXPECT_EQ(e_soa.y, e_std.y);
        EXPECT_EQ(e_soa.p, e_std.p);
        EXPECT_EQ(e_soa.t, e_std.t);
    }
}

TEST(EventsSoATest, EraseTest) {
    EventsSoA soa;
    std::vector<EventType> aos;

    // Fill with 100 elements
    for (int i = 0; i < 100; ++i) {
        EventType e{static_cast<uint16_t>(i), static_cast<uint16_t>(i+1), static_cast<int16_t>(i+2), i*1000LL};
        soa.push_back(e);
        aos.push_back(e);
    }

    // Random engine for choosing erase positions
    std::mt19937 rng(42);
    std::uniform_int_distribution<size_t> dist(0, soa.size()-1);

    // Perform 10 random single-element erases
    for (int k = 0; k < 10; ++k) {
        size_t idx = dist(rng);
        soa.erase(soa.begin() + idx);
        aos.erase(aos.begin() + idx);

        // Verify size
        ASSERT_EQ(soa.size(), aos.size());

        // Verify content matches
        for (size_t i = 0; i < aos.size(); ++i) {
            auto e_soa = soa[i];
            const auto &e_aos = aos[i];
            EXPECT_EQ(e_soa.x, e_aos.x);
            EXPECT_EQ(e_soa.y, e_aos.y);
            EXPECT_EQ(e_soa.p, e_aos.p);
            EXPECT_EQ(e_soa.t, e_aos.t);
        }
    }

    // Perform a range erase
    size_t first = 10, last = 20;
    soa.erase(soa.begin() + first, soa.begin() + last);
    aos.erase(aos.begin() + first, aos.begin() + last);

    ASSERT_EQ(soa.size(), aos.size());
    for (size_t i = 0; i < aos.size(); ++i) {
        auto e_soa = soa[i];
        const auto &e_aos = aos[i];
        EXPECT_EQ(e_soa.x, e_aos.x);
        EXPECT_EQ(e_soa.y, e_aos.y);
        EXPECT_EQ(e_soa.p, e_aos.p);
        EXPECT_EQ(e_soa.t, e_aos.t);
    }
}
