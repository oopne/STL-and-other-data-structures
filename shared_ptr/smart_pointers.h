#include <memory>

struct BaseControlBlock {
    size_t sharedCount = 0;
    size_t weakCount = 0;
    
    virtual void* getPointer() = 0;
    
    virtual void deleteObject() = 0;
    virtual void deallocateBlock() = 0;
    
    virtual ~BaseControlBlock() = default;
};

template <typename T, typename Deleter, typename Alloc>
struct ControlBlockDirect : BaseControlBlock {
    using BlockAlloc = typename std::allocator_traits<Alloc>
        ::template rebind_alloc<ControlBlockDirect>;

    T* pointer;
    Deleter objectDeleter;
    BlockAlloc blockAlloc;
    
    ControlBlockDirect(T* initPointer, Deleter initDeleter, Alloc&& initAlloc)
        : pointer(initPointer), objectDeleter(initDeleter), blockAlloc(std::move(initAlloc))
    {}
    
    void* getPointer() override {
        return pointer;
    }
    
    void deleteObject() override {
        objectDeleter(pointer);
        pointer = nullptr;
    }
    
    void deallocateBlock() override {
        std::allocator_traits<BlockAlloc>::deallocate(blockAlloc, this, 1);
    }
};

template <typename T, typename Alloc>
struct ControlBlockAllocateShared : BaseControlBlock {
    T item;
    Alloc alloc;
    
    template <typename... Args>
    ControlBlockAllocateShared(Alloc&& initAlloc, Args&&... args)
        : item(std::forward<Args>(args)...), alloc(std::move(initAlloc))
    {}
    
    void* getPointer() override {
        return &item;
    }
    
    void deleteObject() override {
        std::allocator_traits<Alloc>::destroy(alloc, &item);
    }
    
    void deallocateBlock() override {
        using BlockAlloc = typename std::allocator_traits<Alloc>
            ::template rebind_alloc<ControlBlockAllocateShared>;
        BlockAlloc blockAlloc(alloc);
        std::allocator_traits<BlockAlloc>::deallocate(blockAlloc, this, 1);
    }
};

template <typename T>
class SharedPtr {
private:
    template <typename U>
    friend class SharedPtr;

    template <typename U>
    friend class WeakPtr;

    template <typename U, typename Alloc, typename... Args>
    friend SharedPtr<U> allocateShared(const Alloc&, Args&&...);

    BaseControlBlock* data_ = nullptr;

    template <typename Alloc, typename... Args>
    SharedPtr(const Alloc& alloc, Args&&... args) {
        using SharedAlloc = typename std::allocator_traits<Alloc>
            ::template rebind_alloc<
                ControlBlockAllocateShared<T, Alloc>
            >;
        using TraitsT = std::allocator_traits<SharedAlloc>;
        SharedAlloc sharedAlloc(alloc);
        data_ = TraitsT::allocate(sharedAlloc, 1);
        std::allocator_traits<SharedAlloc>::construct(
            sharedAlloc,
            reinterpret_cast<ControlBlockAllocateShared<T, Alloc>*>(data_),
            std::move(sharedAlloc),
            std::forward<Args>(args)...
        );
        ++data_->sharedCount;
    }

    SharedPtr(BaseControlBlock* block) : data_(block) {
        if (data_) {
            ++data_->sharedCount;
        }
    }

public:
    SharedPtr() = default;

    template <typename U, typename Deleter, typename Alloc>
    SharedPtr(U* pointer, Deleter deleter, Alloc alloc) {
        using DirectAlloc = typename std::allocator_traits<Alloc>
            ::template rebind_alloc<
                ControlBlockDirect<U, Deleter, Alloc>
            >;
        using TraitsT = std::allocator_traits<DirectAlloc>;
        DirectAlloc directAlloc(alloc);
        data_ = TraitsT::allocate(directAlloc, 1);
        new(reinterpret_cast<ControlBlockDirect<U, Deleter, Alloc>*>(data_))
            ControlBlockDirect<U, Deleter, Alloc>(
                pointer, deleter, std::move(directAlloc));
        ++data_->sharedCount;
    }

    template <typename U, typename Deleter>
    SharedPtr(U* pointer, Deleter deleter)
        : SharedPtr(pointer, deleter, std::allocator<U>())
    {}

    template <typename U>
    explicit SharedPtr(U* pointer)
        : SharedPtr(pointer, std::default_delete<U>(), std::allocator<U>())
    {}

    SharedPtr(const SharedPtr& other) : SharedPtr(other.data_) {}

    template <typename U>
    SharedPtr(const SharedPtr<U>& other) : SharedPtr(other.data_) {}

    template <typename U>
    SharedPtr(SharedPtr<U>&& other) : data_(other.data_) {
        other.data_ = nullptr;
    }

    ~SharedPtr() {
        if (!data_) {
            return;
        }
        --data_->sharedCount;
        if (data_->sharedCount == 0) {
            data_->deleteObject();
            if (data_->weakCount == 0) {
                data_->deallocateBlock();
            }
        }
    }

    template <typename U>
    void swap(SharedPtr<U>& other) {
        std::swap(data_, other.data_);
    }

    template <typename U>
    SharedPtr& operator=(const SharedPtr<U>& other) {
        SharedPtr<U> copy(other);
        swap(copy);
        return *this;
    }

    SharedPtr& operator=(const SharedPtr& other) {
        SharedPtr copy(other);
        swap(copy);
        return *this;
    }

    template <typename U>
    SharedPtr& operator=(SharedPtr<U>&& other) {
        SharedPtr<U> copy(std::move(other));
        swap(copy);
        return *this;
    }

    T* get() const {
        return !data_ ? nullptr : reinterpret_cast<T*>(data_->getPointer());
    }

    size_t use_count() const {
        return !data_ ? 0 : data_->sharedCount;
    }

    void reset() {
        *this = SharedPtr<T>();
    }

    template <typename U>
    void reset(U* pointer) {
        SharedPtr<U> temp(pointer);
        swap(temp);
    }

    T& operator*() const {
        return *get();
    }

    T* operator->() const {
        return get();
    }
};

template <typename T>
class WeakPtr {
private:
    template <typename U>
    friend class WeakPtr;

    BaseControlBlock* data_ = nullptr;
    
    void increment() {
        if (data_) {
            ++data_->weakCount;
        }
    }

public:
    WeakPtr() = default;

    template <typename U>
    WeakPtr(const SharedPtr<U>& other) : data_(other.data_) {
        increment();
    }

    template <typename U>
    WeakPtr(const WeakPtr<U>& other) : data_(other.data_) {
        increment();
    }

    WeakPtr(const WeakPtr& other) : data_(other.data_) {
        increment();
    }

    template <typename U>
    WeakPtr(WeakPtr<U>&& other) : data_(other.data_) {
        other.data_ = nullptr;
    }

    ~WeakPtr() {
        if (!data_) {
            return;
        }
        --data_->weakCount;
        if (data_->sharedCount == 0 && data_->weakCount == 0) {
            data_->deallocateBlock();
        }
    }

    template <typename U>
    void swap(WeakPtr<U>& other) {
        std::swap(data_, other.data_);
    }

    template <typename U>
    WeakPtr& operator=(const WeakPtr<U>& other) {
        WeakPtr copy(other);
        swap(other);
        return *this;
    }

    template <typename U>
    WeakPtr& operator=(WeakPtr<U>&& other) {
        WeakPtr<U> copy(std::move(other));
        swap(copy);
        return *this;
    }

    WeakPtr& operator=(WeakPtr&& other) {
        WeakPtr copy(std::move(other));
        swap(copy);
        return *this;
    }

    SharedPtr<T> lock() const {
        return SharedPtr<T>(expired() ? nullptr : data_);
    }

    size_t use_count() const {
        return !data_ ? 0 : data_->sharedCount;
    }

    bool expired() const {
        return use_count() == 0;
    }
};

template <typename U, typename Alloc, typename... Args>
SharedPtr<U> allocateShared(const Alloc& alloc, Args&&... args) {
    return SharedPtr<U>(alloc, std::forward<Args>(args)...);
}

template <typename U, typename... Args>
SharedPtr<U> makeShared(Args&&... args) {
    return allocateShared<U, std::allocator<U>, Args...>(
        std::allocator<U>(), 
        std::forward<Args>(args)...
    );
}
