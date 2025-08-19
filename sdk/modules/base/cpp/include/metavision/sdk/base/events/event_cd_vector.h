/**********************************************************************************************************************
 * Copyright (c) Prophesee S.A.                                                                                       *
 *                                                                                                                    *
 * Licensed under the Apache License, Version 2.0 (the "License");                                                    *
 * you may not use this file except in compliance with the License.                                                   *
 * You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0                                 *
 * Unless required by applicable law or agreed to in writing, software distributed under the License is distributed   *
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.                      *
 * See the License for the specific language governing permissions and limitations under the License.                 *
 **********************************************************************************************************************/

#ifndef METAVISION_SDK_BASE_EVENT_CD_VECTOR_H
#define METAVISION_SDK_BASE_EVENT_CD_VECTOR_H

#include <cstdint>
#include <iostream>

#include "metavision/sdk/base/utils/timestamp.h"
#include "metavision/sdk/base/events/events_soa.h"

namespace Metavision {

/// @brief Class representing Vectorized 2D CD (Contrast Detection) events:
/// @details vector_mask represents 32 potentially triggered events in a single lane as a 32bit value.
/// Each set bit represents a triggered event at pos(base_x + vector_mask[i], y)
class EventCDVector {
public:
    using timestamp = int64_t;

    EventCDVector() = default;

    /// @brief Construct and immediately push a single event
    EventCDVector(uint16_t x, uint16_t y, bool polarity,
                    uint32_t vector_mask, timestamp t) {
        events_.push_back({x, y, static_cast<int16_t>(polarity), t});
    }

    /// @brief Add a new event directly into the underlying EventsSoA
    void push_back(uint16_t x, uint16_t y, bool polarity, timestamp t) {
        events_.push_back({x, y, static_cast<int16_t>(polarity), t});
    }

    /// @brief Add an EventCD directly
    void push_back(const EventCD &ev) {
        events_.push_back(ev);
    }

    /// @brief Access event at index
    EventsSoA::Reference operator[](std::size_t idx) {
        return events_[idx];
    }

    const EventCD operator[](std::size_t idx) const {
        return events_[idx];
    }

    /// @brief Get the number of events
    std::size_t size() const noexcept { return events_.size(); }
    bool empty() const noexcept { return events_.empty(); }

    /// @brief Clear all events
    void clear() noexcept { events_.clear(); }

    /// @brief Access underlying EventsSoA
    EventsSoA &soa() { return events_; }
    const EventsSoA &soa() const { return events_; }

    /// @brief Stream operator for debugging
    friend std::ostream &operator<<(std::ostream &os, const EventCDVector &vec) {
        os << "EventCDVector with " << vec.size() << " events:\n";
        for (std::size_t i = 0; i < vec.size(); ++i) {
            const auto &e = vec[i];
            os << "(" << e.x << ", " << e.y << ", " << e.p << ", " << e.t << ")\n";
        }
        return os;
    }

    bool operator==(const EventCDVector &other) const {
        return events_ == other.events_;
    }
    
    bool operator!=(const EventCDVector &other) const {
        return events_ != other.events_;
    }

    EventsSoA events_;
};

} // namespace Metavision

// METAVISION_DEFINE_EVENT_TRAIT(Metavision::EventCDVector, 13, "CDVector")

#endif // METAVISION_SDK_BASE_EVENT_CD_VECTOR_H