#include <cstdlib>
#include <cstddef>
#include <memory>

template <size_t N>
class alignas(std::max_align_t) StackStorage {
private:
    char memory_[N];
    size_t position_;

public:
    StackStorage() : position_(0) {}

    StackStorage(const StackStorage&) = delete;

    StackStorage& operator=(const StackStorage&) = delete;

    char* get_memory(size_t count, size_t align) {
        position_ = (position_ + align - 1) / align * align + count;
        return memory_ + position_ - count;
    }
};

template <typename T, size_t N>
class StackAllocator {
private:
    StackStorage<N>* storage_;

public:
    using value_type = T;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;
    using size_type = size_t;
    using difference_type = std::ptrdiff_t;

    template <typename U>
    struct rebind {
        using other = StackAllocator<U, N>;
    };

    StackAllocator() = delete;

    StackAllocator(StackStorage<N>& init_storage) : storage_(&init_storage) {}

    ~StackAllocator() = default;

    template <typename U>
    StackAllocator(const StackAllocator<U, N>& other) noexcept : storage_(other.storage_) {}

    StackAllocator& operator=(const StackAllocator& other) {
        storage_ = other.storage_;
        return *this;
    }

    T* allocate(size_t count) noexcept {
        return reinterpret_cast<T*>(storage_->get_memory(sizeof(T) * count, alignof(T)));
    }

    void deallocate(T*, size_t) noexcept {}

    StackAllocator select_on_container_copy_construction() const {
        return *this;
    }

    template <typename U, size_t M>
    bool operator==(const StackAllocator<U, M>& other) const {
        return true;
    }

    template <typename U, size_t M>
    bool operator!=(const StackAllocator<U, M>& other) const {
        return false;
    }

    template <typename U, size_t M>
    friend class StackAllocator;
};

template <typename T, typename Allocator = std::allocator<T> >
class List {
private:
    struct BaseNode {
        BaseNode* prev;
        BaseNode* next;
    };

    struct Node: BaseNode {
        T value;

        template <typename... Args>
        Node(const Args&... args) : value(args...) {}
    };

    using NodeAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<Node>;

    using traits_t = std::allocator_traits<NodeAllocator>;

    BaseNode fake_object_;
    BaseNode* fake_ = &fake_object_;
    size_t size_;
    NodeAllocator node_allocator_;

    void create_fake_node() {
        fake_->prev = fake_->next = fake_;
    }

    template <typename... Args>
    void emplace(const Args&... args) {
        BaseNode* end = fake_->prev;
        end->next = traits_t::allocate(node_allocator_, 1);
        try {
            traits_t::construct(node_allocator_, static_cast<Node*>(end->next), args...);
        } catch (...) {
            traits_t::deallocate(node_allocator_, static_cast<Node*>(end->next), 1);
            end->next = fake_;
            throw;
        }
        end->next->prev = end;
        end->next->next = fake_;
        fake_->prev = end->next;
    }

    template <typename... Args>
    void construct_with_args(const Args&... args) {
        create_fake_node();
        for (size_t i = 0; i < size_; ++i) {
            try {
                emplace(args...);
            } catch (...) {
                for (size_t j = 0; j < i; ++j) {
                    pop_back();
                }
                throw;
            }
        }
    }
    
    void copy_helper(const List& other) {
        BaseNode* current = other.fake_->next;
        for (size_t i = 0; i < other.size_; ++i) {
            try {
                push_back(static_cast<Node*>(current)->value);
            } catch (...) {
                for (size_t j = 0; j < i; ++j) {
                    pop_back();
                }
                throw;
            }
            current = current->next;
        }
    }

public:
    List() : size_(0) {
        create_fake_node();
    }

    List(size_t count) : size_(count) {
        construct_with_args();
    }

    List(size_t count, const T& item) : size_(count) {
        construct_with_args(item);
    }

    List(Allocator allocator) : size_(0), node_allocator_(allocator) {
        create_fake_node();
    }

    List(size_t count, const T& item, Allocator allocator)
        : size_(count)
        , node_allocator_(allocator)
    {
        construct_with_args(item);
    }

    List(size_t count, Allocator allocator)
        : size_(count)
        , node_allocator_(allocator)
    {
        construct_with_args();
    }

    List(const List& other)
        : size_(0)
        , node_allocator_(traits_t::select_on_container_copy_construction(other.node_allocator_))
    {
        create_fake_node();
        copy_helper(other);
    }

    List& operator=(const List& other) {
        size_t prev_size = size_;
        NodeAllocator prev_alloc = node_allocator_;
        NodeAllocator new_alloc = node_allocator_;
        if constexpr (traits_t::propagate_on_container_copy_assignment::value) {
            node_allocator_ = new_alloc = other.node_allocator_;
        }
        try {
            copy_helper(other);
        } catch (...) {
            node_allocator_ = prev_alloc;
            throw;
        }
        node_allocator_ = prev_alloc;
        for (size_t i = 0; i < prev_size; ++i) {
            pop_front();
        }
        node_allocator_ = new_alloc;
        size_ = other.size_;
        return *this;
    }

    ~List() {
        while (size_ > 0) {
            pop_back();
        }
    }

    NodeAllocator get_allocator() const {
        return node_allocator_;
    }

    size_t size() const {
        return size_;
    }

    void push_back(const T& item) {
        insert(end(), item);
    }

    void push_front(const T& item) {
        insert(begin(), item);
    }

    void pop_back() {
        iterator iter = end();
        erase(--iter);
    }

    void pop_front() {
        erase(begin());
    }

    template <bool IsConst>
    class CommonIterator {
    private:
        BaseNode* position_;

    public:
        using value_type = std::conditional_t<IsConst, const T, T>;
        using pointer = std::conditional_t<IsConst, const T*, T*>;
        using reference = std::conditional_t<IsConst, const T&, T&>;
        using difference_type = std::ptrdiff_t;
        using iterator_category = std::bidirectional_iterator_tag;

        CommonIterator(BaseNode* position) : position_(position) {}

        CommonIterator(const CommonIterator<false>& other)
            : position_(other.position_) {}

        CommonIterator& operator=(const CommonIterator<false>& other) {
            position_ = other.position_;
            return *this;
        }

        ~CommonIterator() = default;

        CommonIterator& operator++() {
            position_ = position_->next;
            return *this;
        }

        CommonIterator operator++(int) {
            CommonIterator result = *this;
            position_ = position_->next;
            return result;
        }

        CommonIterator& operator--() {
            position_ = position_->prev;
            return *this;
        }

        CommonIterator operator--(int) {
            CommonIterator result = *this;
            position_ = position_->prev;
            return result;
        }

        reference operator*() const {
            return static_cast<Node*>(position_)->value;
        }

        template <bool IsConstOther>
        bool operator==(CommonIterator<IsConstOther> other) const {
            return position_ == other.position_;
        }

        template <bool IsConstOther>
        bool operator!=(CommonIterator<IsConstOther> other) const {
            return !(*this == other);
        }

        template <bool IsConstOther>
        friend class CommonIterator;

        friend class List;
    };

    using iterator = CommonIterator<false>;
    using const_iterator = CommonIterator<true>;

    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    iterator begin() const noexcept {
        return iterator(fake_->next);
    }

    iterator end() const noexcept {
        return iterator(fake_);
    }

    const_iterator cbegin() const noexcept {
        return const_iterator(fake_->next);
    }

    const_iterator cend() const noexcept {
        return const_iterator(fake_);
    }

    reverse_iterator rbegin() const noexcept {
        return reverse_iterator(iterator(fake_));
    }

    reverse_iterator rend() const noexcept {
        return reverse_iterator(iterator(fake_->next));
    }

    const_reverse_iterator crbegin() const noexcept {
        return const_reverse_iterator(const_iterator(fake_));
    }

    const_reverse_iterator crend() const noexcept {
        return const_reverse_iterator(const_iterator(fake_->next));
    }

    iterator insert(const_iterator iter, const T& item) {
        BaseNode* after = iter.position_;
        BaseNode* inserted = traits_t::allocate(node_allocator_, 1);
        try {
            traits_t::construct(node_allocator_, static_cast<Node*>(inserted), item);
        } catch (...) {
            traits_t::deallocate(node_allocator_, static_cast<Node*>(inserted), 1);
            throw;
        }
        after->prev->next = inserted;
        inserted->prev = after->prev;
        inserted->next = after;
        after->prev = inserted;
        ++size_;
        return iterator(inserted);
    }

    void erase(const_iterator iter) {
        BaseNode* to_delete = iter.position_;
        BaseNode* before = to_delete->prev;
        BaseNode* after = to_delete->next;
        traits_t::destroy(node_allocator_, static_cast<Node*>(to_delete));
        traits_t::deallocate(node_allocator_, static_cast<Node*>(to_delete), 1);
        before->next = after;
        after->prev = before;
        --size_;
    }
};
