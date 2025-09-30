#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;
    RawMemory(const RawMemory& other) = delete;
    RawMemory& operator=(const RawMemory& other) = delete;

    RawMemory(RawMemory&& other) noexcept{
        Swap(other);
    }

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    RawMemory& operator=(RawMemory&& other) noexcept {
        if (this != &other) {
            Swap(other);
        }
        return *this;
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:
    using iterator = T*;
    using const_iterator = const T*;
    Vector() = default;

    Vector(size_t size)
        :data_(size),
        size_(size) {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(const Vector& other)
        :data_(other.size_),
        size_(other.size_) {
        std::uninitialized_copy_n(other.data_.GetAddress(), other.size_, data_.GetAddress());
    }

    Vector(Vector&& other) noexcept
    {
        Swap(other);
    }

    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }

    iterator begin() noexcept {
        return data_.GetAddress();
    }

    iterator end() noexcept {
        return data_.GetAddress() + size_;
    }

    const_iterator begin() const noexcept {
        return const_iterator(data_.GetAddress());
    }

    const_iterator end() const noexcept {
        return const_iterator(data_.GetAddress() + size_);
    }

    const_iterator cbegin() const noexcept {
        return begin();
    }

    const_iterator cend() const noexcept {
        return end();
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    void CopyAssign(const Vector& other){
        size_t min_size = std::min(size_, other.size_);
        std::copy(other.data_.GetAddress(), other.data_.GetAddress() + min_size, data_.GetAddress());
        if(min_size == other.size_){
            std::destroy_n(data_.GetAddress() + other.size_, size_ - other.size_);  
        }  
        else {
            std::uninitialized_copy_n(other.data_.GetAddress() + size_, other.size_ - size_, data_.GetAddress() + size_);  
        } 
    }
    
    Vector& operator=(const Vector& other) {
        if(this != &other){
            if(other.size_ > data_.Capacity()){
                Vector copy_other(other);
                Swap(copy_other);
            }
            else {
                CopyAssign(other);
            }
        }
        size_ = other.size_;
        return *this;
    }

    Vector& operator=(Vector&& other) noexcept {
        if (this != &other) {
            Swap(other);
        }
        return *this;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_.GetAddress()[index];
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }
        RawMemory<T> new_data{new_capacity};
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        else std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());

        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }

    void Resize(size_t new_size) {
        if (new_size < size_) {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
        }
        else {
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
        }
        size_ = new_size;
    }

    template<typename... Args>
    void PushBack(Args&&... args) {
        EmplaceBack(std::forward<Args>(args)...);
    }

    void PopBack() noexcept {
        assert(size_ != 0);
        const size_t ONE = 1;
        std::destroy_n(data_.GetAddress() + size_ - ONE, ONE);
        --size_;
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        if (size_ == Capacity()) {
            RawMemory<T> new_data(size_ == 0 ? 1 : Capacity() * 2);
            new(new_data.GetAddress() + size_) T(std::forward<Args>(args)...);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            else {
                try{
                    std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
                }
                catch(...){
                    std::destroy_n(new_data.GetAddress(), size_);
                    throw;
                }
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        }
        else if (size_ < Capacity()) {
            new(data_.GetAddress() + size_) T(std::forward<Args>(args)...);
        }
        size_t new_size_ = size_;
        ++size_;
        return data_[new_size_];

    }

    template<typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        assert(pos >= begin() && pos <= end());
        size_t count = pos - begin();
        if (size_ < Capacity()) {
            if (pos == end()) {
                std::construct_at<T>(data_.GetAddress() + size_, std::forward<Args>(args)...);
            }
            else {
                assert(size_ > 0);
                T temp(std::forward<Args>(args)...);
                new(data_.GetAddress() + size_) T(std::move(*(data_.GetAddress() + size_ - 1)));
                try {
                    std::move_backward(begin() + count, data_.GetAddress() + size_ - 1, data_.GetAddress() + size_);
                }
                catch (...) {
                    std::destroy_n(data_.GetAddress() + size_, 1);
                    throw;
                }
                try {
                    data_[count] = std::move(temp);
                }
                catch(...){
                    std::destroy_n(data_.GetAddress() + count, 1);
                }
            }
        }
        else {
            RawMemory<T> new_data(size_ == 0 ? 1 : Capacity() * 2);
            try {
                new(new_data.GetAddress() + count) T(std::forward<Args>(args)...);
            }
            catch (...) {
                std::destroy_n(new_data.GetAddress() + count, 1);
                throw;
            }

            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), count, new_data.GetAddress());
                std::uninitialized_move_n(data_.GetAddress() + count, size_ - count, new_data.GetAddress() + count + 1);
            }
            else {
                try
                {
                    std::uninitialized_copy_n(data_.GetAddress(), count, new_data.GetAddress());
                    std::uninitialized_copy_n(data_.GetAddress() + count, size_ - count, new_data.GetAddress() + count + 1);
                }
                catch (...) {
                    std::destroy_n(new_data.GetAddress(), size_);
                    throw;
                }
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        }
        ++size_;
        return begin() + count;
    }

    iterator Erase(const_iterator pos) noexcept(std::is_nothrow_move_assignable_v<T>) {
        assert(pos < end() && pos >= begin());
        auto count = pos - begin();
        std::move(data_.GetAddress() + count + 1, data_.GetAddress() + size_, data_.GetAddress() + count);
        PopBack();
        return begin() + count;
    }

    template<typename... Args>
    iterator Insert(Args&&... args) {
        return Emplace(std::forward<Args>(args)...);
    }

    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

private:

    static void CopyConstruct(T* buf, const T& elem) {
        new(buf) T(elem);
    }

    RawMemory<T> data_;
    size_t size_ = 0;
};
