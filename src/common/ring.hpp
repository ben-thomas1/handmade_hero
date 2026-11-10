#pragma once

#include "common/types.h"

#include <cassert>
#include <cstring>


template <typename T>
struct Ring {
    T *data;
    u32 read;
    u32 write;
    u32 len;
    u32 cap;

    void init(T *storage, u32 capacity);
    bool push(T value);
    T pop();
    T peek() const;
    void push_block(const T *src, u32 n);
    void pop_block(T *dst, u32 n);
    bool empty() const;
    u32 remaining() const;
};


template <typename T>
void
Ring<T>::init(T *storage, u32 capacity)
{
    this->data = storage;
    this->read = 0;
    this->write = 0;
    this->len = 0;
    this->cap = capacity;
}

template <typename T>
bool
Ring<T>::push(T value)
{
    if (this->len == this->cap) return false;
    this->data[this->write++] = value;
    this->write %= this->cap;
    this->len++;
    return true;
}

template <typename T>
T
Ring<T>::pop()
{
    assert(!this->empty());
    T value = this->data[this->read++];
    this->read %= this->cap;
    this->len--;
    return value;
}

template <typename T>
T
Ring<T>::peek() const
{
    if (this->len == 0) return T{};
    return this->data[this->read];
}

template <typename T>
void
Ring<T>::push_block(const T *src, u32 n)
{
    if (n == 0) return;
    assert(n <= this->remaining());
    u32 first = this->cap - this->write;
    if (first > n) first = n;
    u32 second = n - first;

    std::memcpy(this->data + this->write, src, first * sizeof(T));
    if (second > 0) {
        std::memcpy(this->data, src + first, second * sizeof(T));
    }
    this->len += n;
    this->write += n;
    this->write %= this->cap;
}

template <typename T>
void
Ring<T>::pop_block(T *dst, u32 n)
{
    if (n == 0) return;
    assert(n <= this->len);
    u32 first = this->cap - this->read;
    if (first > n) first = n;
    u32 second = n - first;

    std::memcpy(dst, this->data + this->read, first * sizeof(T));
    if (second > 0) {
        std::memcpy(dst + first, this->data, second * sizeof(T));
    }
    this->len -= n;
    this->read += n;
    this->read %= this->cap;
}

template <typename T>
bool
Ring<T>::empty() const
{
    return this->len == 0;
}

template <typename T>
u32
Ring<T>::remaining() const
{
    return this->cap - this->len;
}
