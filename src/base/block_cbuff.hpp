// TODO: memcpy all these buffers

#pragma once

#include "base/types.hpp"

#include <cassert>
#include <cstring>


template <typename T>
struct BlockCBuff {
    T *buffer;
    u32 read;
    u32 write;
    u32 len;
    u32 cap;

    void init(T *buffer, u32 buff_size);
    void push_block(T *src, u32 n);
    void pop_block(T *dst, u32 n);
    u32 remaining();
};


template <typename T>
void
BlockCBuff<T>::init(T *buffer, u32 buff_size)
{
    this->buffer = buffer;
    this->len = 0;
    this->cap = buff_size;
    this->read = 0;
    this->write = 0;
}

template <typename T>
void
BlockCBuff<T>::push_block(T *src, u32 n)
{
    if (n == 0) return;
    assert(n <= this->remaining());
    u32 first = this->cap - this->write;
    if (first > n) first = n;
    u32 second = n - first;

    std::memcpy(this->buffer + this->write, src, first * sizeof(T));
    if (second > 0) {
        std::memcpy(this->buffer, src + first, second * sizeof(T));
    }
    this->len += n;
    this->write += n;
    this->write %= this->cap;
}

template <typename T>
void
BlockCBuff<T>::pop_block(T *dst, u32 n)
{
    if (n == 0) return;
    assert(n <= this->len);
    u32 first = this->cap - this->read;
    if (first > n) first = n;
    u32 second = n - first;

    std::memcpy(dst, this->buffer + this->read, first * sizeof(T));
    if (second > 0) {
        std::memcpy(dst + first, this->buffer, second * sizeof(T));
    }
    this->len -= n;
    this->read += n;
    this->read %= this->cap;
}

template <typename T>
u32
BlockCBuff<T>::remaining()
{
    return this->cap - this->len;
}
