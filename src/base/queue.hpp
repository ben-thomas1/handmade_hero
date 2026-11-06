#pragma once

#include "base/types.hpp"

#include <cassert>

template <typename T>
struct Queue {
    T *buffer;
    u32 front;
    u32 end;
    u32 len;
    u32 cap;

    void init(T *buffer, u32 buff_size);
    bool enqueue(T val);
    // Precondition: !empty(). Dequeuing an empty queue is undefined behaviour.
    T dequeue();
    T peek();
    bool empty();
};


template <typename T>
void
Queue<T>::init(T *buffer, u32 buff_size)
{
    this->buffer = buffer;
    this->len = 0;
    this->cap = buff_size;
    this->front = 0;
    this->end = 0;
}

template <typename T>
bool
Queue<T>::enqueue(T val)
{
    if (this->len == this->cap) return false;
    this->buffer[this->end++] = val;
    this->end %= this->cap;
    this->len++;
    return true;
}

template <typename T>
T
Queue<T>::dequeue()
{
    assert(!this->empty());
    T val = this->buffer[this->front++];
    this->front %= this->cap;
    this->len--;
    return val;
}

template <typename T>
T
Queue<T>::peek()
{
    if (this->len == 0) return T{}; // failed
    return this->buffer[this->front];
}

template <typename T>
bool
Queue<T>::empty()
{
    return this->len == 0;
}
