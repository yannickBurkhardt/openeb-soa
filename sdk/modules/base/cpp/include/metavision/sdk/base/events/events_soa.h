#pragma once

#include <vector>
#include <cstddef>
#include <stdexcept>
#include <iterator>

#include "metavision/sdk/base/events/event_cd.h"

namespace Metavision {
using EventType = EventCD;

class EventsSoA {

public:
    using size_type = std::size_t;
    using value_type = EventType;
    static constexpr size_type kDefaultReservationSize = 50'000'000;
    inline static const char* kReservationSizeFromEnv = std::getenv("OPENEB_MAX_RESERVATION_SIZE");
    inline static const size_type kReservationSize = kReservationSizeFromEnv ? 
        static_cast<size_type>(std::atol(kReservationSizeFromEnv)) :
        kDefaultReservationSize;

    EventsSoA (size_type n_events = kReservationSize) {
        reserve(n_events);
    }

    std::vector<uint16_t> xs;
    std::vector<uint16_t> ys;
    std::vector<int16_t> ps;
    std::vector<int64_t> ts;

    // Proxy reference for element access
    struct Reference {
        uint16_t &x;
        uint16_t &y;
        int16_t &p;
        int64_t &t;

        Reference(uint16_t &xr, uint16_t &yr, int16_t &pr, int64_t &tr)
            : x(xr), y(yr), p(pr), t(tr) {}

        Reference &operator=(const EventType &val) {
            x = val.x; y = val.y; p = val.p; t = val.t;
            return *this;
        }

        operator EventType() const {
            return EventType{x, y, p, t};
        }
    };

    // Basic vector-like API
    size_type size() const noexcept { return xs.size(); }
    bool empty() const noexcept { return xs.empty(); }
    void reserve(size_type n) {
        xs.reserve(n); ys.reserve(n); ps.reserve(n); ts.reserve(n);
    }
    void resize(size_type n) {
        xs.resize(n); ys.resize(n); ps.resize(n); ts.resize(n);
    }
    void clear() noexcept {
        xs.clear(); ys.clear(); ps.clear(); ts.clear();
    }

    void push_back(const EventType &val) {
        xs.push_back(val.x);
        ys.push_back(val.y);
        ps.push_back(val.p);
        ts.push_back(val.t);
    }

    template<typename... Args>
    std::enable_if_t<sizeof...(Args) == 4>
    emplace_back(Args&&... args) {
        emplace_back_impl(std::forward<Args>(args)...);
    }
    
    void emplace_back(const EventType &e) {
        emplace_back_impl(e.x, e.y, e.p, e.t);
    }
    
    void emplace_back(EventType &&e) {
        emplace_back_impl(e.x, e.y, e.p, e.t);
    }
    

    void pop_back() {
        xs.pop_back(); ys.pop_back(); ps.pop_back(); ts.pop_back();
    }

    EventType operator[](size_type idx) const {
        return { xs[idx], ys[idx], ps[idx], ts[idx] };
    }

    Reference operator[](size_type idx) {
        return Reference(xs[idx], ys[idx], ps[idx], ts[idx]);
    }

    EventType at(size_type idx) const {
        if (idx >= size()) throw std::out_of_range("EventsSoA::at");
        return { xs.at(idx), ys.at(idx), ps.at(idx), ts.at(idx) };
    }

    Reference at(size_type idx) {
        if (idx >= size()) throw std::out_of_range("EventsSoA::at");
        return Reference(xs.at(idx), ys.at(idx), ps.at(idx), ts.at(idx));
    }

    bool operator==(const EventsSoA &other) const {
        return xs == other.xs &&
               ys == other.ys &&
               ps == other.ps &&
               ts == other.ts;
    }
    
    bool operator!=(const EventsSoA &other) const {
        return !(*this == other);
    }

    struct Iterator {
        EventsSoA *parent{nullptr};
        size_type index{0};
    
        using iterator_category = std::random_access_iterator_tag;
        using value_type = EventType;
        using difference_type = std::ptrdiff_t;
        using pointer = void;
        using reference = EventsSoA::Reference; // return proxy by value
    
        Iterator() = default;
        Iterator(EventsSoA *p, size_type i) : parent(p), index(i) {}
    
        reference operator*() const {
            // construct a proxy on the fly, no cache needed
            return reference(parent->xs[index],
                             parent->ys[index],
                             parent->ps[index],
                             parent->ts[index]);
        }
    
        Iterator& operator++() { ++index; return *this; }
        Iterator operator++(int) { Iterator tmp = *this; ++(*this); return tmp; }
        Iterator& operator--() { --index; return *this; }
        Iterator operator--(int) { Iterator tmp = *this; --(*this); return tmp; }
        Iterator& operator+=(difference_type n) { index += n; return *this; }
        Iterator& operator-=(difference_type n) { index -= n; return *this; }
    
        friend Iterator operator+(Iterator it, difference_type n) { it += n; return it; }
        friend Iterator operator-(Iterator it, difference_type n) { it -= n; return it; }
        friend difference_type operator-(const Iterator &a, const Iterator &b) { return a.index - b.index; }
    
        friend bool operator==(const Iterator &a, const Iterator &b) { return a.index == b.index; }
        friend bool operator!=(const Iterator &a, const Iterator &b) { return a.index != b.index; }
        friend bool operator<(const Iterator &a, const Iterator &b) { return a.index < b.index; }
        friend bool operator<=(const Iterator &a, const Iterator &b) { return a.index <= b.index; }
        friend bool operator>(const Iterator &a, const Iterator &b) { return a.index > b.index; }
        friend bool operator>=(const Iterator &a, const Iterator &b) { return a.index >= b.index; }
    };
    

    Iterator begin() { return Iterator(this, 0); }
    Iterator end() { return Iterator(this, size()); }

    Reference front() {
        return Reference(xs.front(), ys.front(), ps.front(), ts.front());
    }

    const EventType front() const {
        return EventType{xs.front(), ys.front(), ps.front(), ts.front()};
    }

    Reference back() {
        return Reference(xs.back(), ys.back(), ps.back(), ts.back());
    }

    const EventType back() const {
        return EventType{xs.back(), ys.back(), ps.back(), ts.back()};
    }

    Iterator insert(Iterator pos, const EventType &val) {
        auto idx = pos.index;
        xs.insert(xs.begin() + idx, val.x);
        ys.insert(ys.begin() + idx, val.y);
        ps.insert(ps.begin() + idx, val.p);
        ts.insert(ts.begin() + idx, val.t);
        return Iterator(this, idx);
    }

    Iterator insert(Iterator pos, EventType &&val) {
        auto idx = pos.index;
        xs.insert(xs.begin() + idx, std::move(val.x));
        ys.insert(ys.begin() + idx, std::move(val.y));
        ps.insert(ps.begin() + idx, std::move(val.p));
        ts.insert(ts.begin() + idx, std::move(val.t));
        return Iterator(this, idx);
    }

    template <class InputIt>
    Iterator insert(Iterator pos, InputIt first, InputIt last) {
        auto idx = pos.index;
        // Reserve space to avoid repeated allocations
        auto count = std::distance(first, last);
        xs.reserve(xs.size() + count);
        ys.reserve(ys.size() + count);
        ps.reserve(ps.size() + count);
        ts.reserve(ts.size() + count);

        std::vector<uint16_t> tmp_x;
        std::vector<uint16_t> tmp_y;
        std::vector<int16_t> tmp_p;
        std::vector<int64_t> tmp_t;

        tmp_x.reserve(count);
        tmp_y.reserve(count);
        tmp_p.reserve(count);
        tmp_t.reserve(count);

        for (auto it = first; it != last; ++it) {
            const auto &val = *it;
            tmp_x.push_back(val.x);
            tmp_y.push_back(val.y);
            tmp_p.push_back(val.p);
            tmp_t.push_back(val.t);
        }

        xs.insert(xs.begin() + pos.index, tmp_x.begin(), tmp_x.end());
        ys.insert(ys.begin() + pos.index, tmp_y.begin(), tmp_y.end());
        ps.insert(ps.begin() + pos.index, tmp_p.begin(), tmp_p.end());
        ts.insert(ts.begin() + pos.index, tmp_t.begin(), tmp_t.end());

        return Iterator(this, pos.index);
    }

    Iterator erase(Iterator pos) {
        size_t index = pos - begin();  // compute the index
        xs.erase(xs.begin() + index);
        ys.erase(ys.begin() + index);
        ps.erase(ps.begin() + index);
        ts.erase(ts.begin() + index);
        return Iterator(this, index); // return custom iterator at next element
    }
    
    Iterator erase(Iterator first, Iterator last) {
        size_t index_first = first - begin();
        size_t index_last  = last - begin();
        xs.erase(xs.begin() + index_first, xs.begin() + index_last);
        ys.erase(ys.begin() + index_first, ys.begin() + index_last);
        ps.erase(ps.begin() + index_first, ps.begin() + index_last);
        ts.erase(ts.begin() + index_first, ts.begin() + index_last);
        return Iterator(this, index_first); // return iterator to first remaining element
    }
    

    // Const iterators
    struct const_iterator {
        const EventsSoA *parent;
        size_type index;
        mutable EventType val_cache;
    
        using iterator_category = std::random_access_iterator_tag;
        using value_type = EventType;
        using difference_type = std::ptrdiff_t;
        using pointer = const value_type*;
        using reference = const value_type&; // return by const ref
    
        const_iterator() = default;

        const_iterator(const EventsSoA *p, size_type i)
            : parent(p), index(i), val_cache((*p)[i]) {}
    
        reference operator*() const {
            val_cache = (*parent)[index];
            return val_cache;
        }
    
        const_iterator& operator++() { ++index; return *this; }
        const_iterator operator++(int) { const_iterator tmp = *this; ++(*this); return tmp; }
        const_iterator& operator--() { --index; return *this; }
        const_iterator operator--(int) { const_iterator tmp = *this; --(*this); return tmp; }
        const_iterator& operator+=(difference_type n) { index += n; return *this; }
        const_iterator& operator-=(difference_type n) { index -= n; return *this; }
        friend const_iterator operator+(const_iterator it, difference_type n) { it += n; return it; }
        friend const_iterator operator-(const_iterator it, difference_type n) { it -= n; return it; }
        friend difference_type operator-(const const_iterator &a, const const_iterator &b) { return a.index - b.index; }
        friend bool operator==(const const_iterator &a, const const_iterator &b) { return a.index == b.index; }
        friend bool operator!=(const const_iterator &a, const const_iterator &b) { return a.index != b.index; }
        friend bool operator<(const const_iterator &a, const const_iterator &b) { return a.index < b.index; }
        friend bool operator>(const const_iterator &a, const const_iterator &b) { return a.index > b.index; }
        friend bool operator<=(const const_iterator &a, const const_iterator &b) { return a.index <= b.index; }
        friend bool operator>=(const const_iterator &a, const const_iterator &b) { return a.index >= b.index; }
    };    

    const_iterator begin() const { return const_iterator(this, 0); }
    const_iterator end() const { return const_iterator(this, size()); }

    const_iterator cbegin() const { return const_iterator(this, 0); }
    const_iterator cend() const { return const_iterator(this, size()); }

    void swap(EventsSoA &other) noexcept {
        using std::swap;
        swap(xs, other.xs);
        swap(ys, other.ys);
        swap(ps, other.ps);
        swap(ts, other.ts);
    }

    // Not supported, throw error
    EventType* data() {
        throw std::runtime_error("Not supported, data is not contiguous.");
    }

    // For printing content
    friend std::ostream &operator<<(std::ostream &os, const EventsSoA &soa) {
        os << "EventsSoA(size=" << soa.size() << ")\n";
        for (std::size_t i = 0; i < soa.size(); ++i) {
            os << "("
               << soa.xs[i] << ", "
               << soa.ys[i] << ", "
               << static_cast<int>(soa.ps[i]) << ", "
               << soa.ts[i]
               << ")";
            if (i + 1 < soa.size())
                os << ", ";
        }
        return os;
    }

private:
    void emplace_back_impl(uint16_t x, uint16_t y, int16_t p, int64_t t) {
        xs.emplace_back(x);
        ys.emplace_back(y);
        ps.emplace_back(p);
        ts.emplace_back(t);
    }
};

} // namespace Metavision

// METAVISION_DEFINE_EVENT_TRAIT(Metavision::EventCD, 12, "CD")
