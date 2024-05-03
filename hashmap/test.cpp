#include <functional>
#include <memory>
#include <stdexcept>
#include <list>
#include <utility>
#include <cstdlib>
#include <cstddef>

#include <iostream>
#include <cassert>

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
    BaseNode* fake_;
    size_t size_;
    NodeAllocator node_allocator_;

    void create_fake_node() {
        fake_->prev = fake_->next = fake_;
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
    List() : fake_(&fake_object_), size_(0) {
        create_fake_node();
    }

    List(Allocator allocator)
        : fake_(&fake_object_)
        , size_(0)
        , node_allocator_(allocator)
    {
        create_fake_node();
    }

    List(const List& other)
        : fake_(&fake_object_)
        , size_(0)
        , node_allocator_(traits_t::select_on_container_copy_construction(other.node_allocator_))
    {
        create_fake_node();
        copy_helper(other);
    }

    List(List&& other)
        : fake_object_(std::move(other.fake_object_))
        , fake_(&fake_object_)
        , size_(other.size_)
        , node_allocator_(other.node_allocator_)
    {
        fake_->next->prev = fake_;
        fake_->prev->next = fake_;
        other.fake_ = nullptr;
        other.size_ = 0;
    }

    List& operator=(const List& other) {
        size_t prev_size = size_;
        NodeAllocator prev_alloc = node_allocator_;
        if constexpr (traits_t::propagate_on_container_copy_assignment::value) {
            node_allocator_ = other.node_allocator_;
        }
        try {
            copy_helper(other);
        } catch (...) {
            node_allocator_ = prev_alloc;
            throw;
        }
        for (size_t i = 0; i < prev_size; ++i) {
            pop_front();
        }
        size_ = other.size_;
        return *this;
    }

    List& operator=(List&& other) {
        // когда вызывается оператор перемещения, лист должен быть пуст
        fake_object_ = std::move(other.fake_object_);
        fake_ = &fake_object_;
        size_ = other.size_;
        fake_->next->prev = fake_;
        fake_->prev->next = fake_;
        other.fake_ = nullptr;
        other.size_ = 0;
        return *this;
    }

    void clear() {
        while (size_ > 0) {
            pop_back();
        }
    }

    ~List() {
        clear();
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

        pointer operator->() const {
            return &(static_cast<Node*>(position_)->value);
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

    template <typename... Args>
    iterator insert(const_iterator iter, Args&&... args) {
        BaseNode* after = iter.position_;
        BaseNode* inserted = traits_t::allocate(node_allocator_, 1);
        try {
            traits_t::construct(
                node_allocator_,
                static_cast<Node*>(inserted), 
                std::forward<Args>(args)...
            );
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

    iterator erase(const_iterator iter) {
        BaseNode* to_delete = iter.position_;
        BaseNode* before = to_delete->prev;
        BaseNode* after = to_delete->next;
        traits_t::destroy(node_allocator_, static_cast<Node*>(to_delete));
        traits_t::deallocate(node_allocator_, static_cast<Node*>(to_delete), 1);
        before->next = after;
        after->prev = before;
        --size_;
        return iterator(after);
    }

    void splice(const_iterator pos, const_iterator it) {
        BaseNode* node = it.position_;
        BaseNode* this_before = pos.position_->prev;
        BaseNode* this_after = pos.position_;
        BaseNode* other_before = node->prev;
        BaseNode* other_after = node->next;
        if (node == this_after) {
            return;
        }
        node->prev = this_before;
        node->next = this_after;
        this_before->next = node;
        this_after->prev = node;
        other_before->next = other_after;
        other_after->prev = other_before;
    }
};

template <
    typename Key,
    typename Value,
    typename Hash = std::hash<Key>,
    typename Equal = std::equal_to<Key>,
    typename Alloc = std::allocator<std::pair<const Key, Value> >
>
class UnorderedMap {
public:
    using NodeType = std::pair<const Key, Value>;

private:
    using traits_t = std::allocator_traits<Alloc>;

    struct ListNode {
        NodeType* key_value;
        size_t hash;

        ListNode(NodeType* ptr, size_t hashed)
            : key_value(ptr)
            , hash(hashed)
        {}
    };

    using ListAlloc = typename traits_t::template rebind_alloc<ListNode>;

    using ListType = List<ListNode, ListAlloc>;
    using ListTypeIter = typename ListType::iterator;

    using ListTypeIterAlloc = typename traits_t::template rebind_alloc<ListTypeIter>;
    using VectorType = typename std::vector<ListTypeIter, ListTypeIterAlloc>;

    static constexpr double DEFAULT_MAX_LOAD_FACTOR = 1;
    static const size_t DEFAULT_BUCKET_COUNT = 1;

    ListType elements_;
    VectorType iterators_;
    
    Hash hash_;
    Equal equal_;
    Alloc alloc_;

    double max_load_factor_;

public:
    template <bool IsConst>
    class CommonIterator {
    private:
        ListTypeIter position_;

    public:
        using value_type = std::conditional_t<IsConst, const NodeType, NodeType>;
        using pointer = std::conditional_t<IsConst, const NodeType*, NodeType*>;
        using reference = std::conditional_t<IsConst, const NodeType&, NodeType&>;
        using difference_type = std::ptrdiff_t;
        using iterator_category = std::forward_iterator_tag;

        CommonIterator(const ListTypeIter& position)
            : position_(position)
        {}

        CommonIterator(const CommonIterator<false>& other)
            : position_(other.position_)
        {}

        CommonIterator& operator=(const CommonIterator<false>& other) {
            position_ = other.position_;
            return *this;
        }

        ~CommonIterator() = default;

        CommonIterator& operator++() {
            ++position_;
            return *this;
        }

        CommonIterator operator++(int) {
            CommonIterator result = *this;
            ++position_;
            return result;
        }

        reference operator*() const {
            return *(position_->key_value);
        }

        template <bool IsConstOther>
        bool operator==(CommonIterator<IsConstOther> other) const {
            return position_ == other.position_;
        }

        template <bool IsConstOther>
        bool operator!=(CommonIterator<IsConstOther> other) const {
            return !(*this == other);
        }

        pointer operator->() const {
            return position_->key_value;
        }

        template <bool IsConstOther>
        friend class CommonIterator;
        
        friend class UnorderedMap;
    };

    using Iterator = CommonIterator<false>;
    using ConstIterator = CommonIterator<true>;

    Iterator begin() noexcept {
        return Iterator(elements_.begin());
    }

    Iterator end() noexcept {
        return Iterator(elements_.end());
    }

    ConstIterator cbegin() noexcept {
        return ConstIterator(elements_.begin());
    }

    ConstIterator cend() noexcept {
        return ConstIterator(elements_.end());
    }

    explicit UnorderedMap(size_t bucket_count,
                          const Hash& hash = Hash(),
                          const Equal& equal = Equal(),
                          const Alloc& alloc = Alloc())
        : elements_(ListAlloc(alloc))
        , iterators_(bucket_count, elements_.end(), ListTypeIterAlloc(alloc))
        , hash_(hash)
        , equal_(equal)
        , alloc_(alloc)
        , max_load_factor_(DEFAULT_MAX_LOAD_FACTOR)
    {}

    UnorderedMap() : UnorderedMap(DEFAULT_BUCKET_COUNT) {}

    UnorderedMap(size_t bucket_count, const Alloc& alloc)
        : UnorderedMap(bucket_count, Hash(), Equal(), alloc)
    {}

    UnorderedMap(size_t bucket_count, const Hash& hash, const Alloc& alloc)
        : UnorderedMap(bucket_count, hash, Equal(), alloc)
    {}

    explicit UnorderedMap(const Alloc& alloc)
        : UnorderedMap(DEFAULT_BUCKET_COUNT, Hash(), Equal(), alloc)
    {}

    UnorderedMap(const UnorderedMap& other) : UnorderedMap() {
        for (auto it = other.elements_.begin(); it != other.elements_.end(); ++it) {
            emplace(*(it->key_value));
        }
    }

    UnorderedMap(UnorderedMap&& other)
        : elements_(std::move(other.elements_))
        , iterators_(std::move(other.iterators_))
        , alloc_(traits_t::select_on_container_copy_construction(other.alloc_))
        , max_load_factor_(std::exchange(other.max_load_factor_,
                                         DEFAULT_MAX_LOAD_FACTOR))
    {}

    ~UnorderedMap() {
        while (elements_.size() > 0) {
            erase(begin());
        }
    }

    UnorderedMap& operator=(const UnorderedMap& other) {
        if constexpr (traits_t::propagate_on_container_copy_assignment::value) {
            alloc_ = other.alloc_;
        }
        while (elements_.size() > 0) {
            erase(begin());
        }
        iterators_.resize(DEFAULT_BUCKET_COUNT, elements_.end());
        for (auto it = other.elements_.begin(); it != other.elements_.end(); ++it) {
            emplace(*(it->key_value));
        }
        max_load_factor_ = other.max_load_factor_;
        return *this;
    }

    UnorderedMap& operator=(UnorderedMap&& other) {
        while (elements_.size() > 0) {
            erase(begin());
        }
        elements_ = std::move(other.elements_);
        iterators_ = std::move(other.iterators_);
        if constexpr (traits_t::propagate_on_container_move_assignment::value) {
            alloc_ = other.alloc_;
        }
        max_load_factor_ = std::exchange(other.max_load_factor_,
                                         DEFAULT_MAX_LOAD_FACTOR);
        return *this;
    }

    Iterator find(const Key& key) {
        size_t index = hash_(key) % iterators_.size();
        ListTypeIter iter = iterators_[index];
        while (iter != elements_.end()
               && iter->hash % iterators_.size() == index) {
            if (equal_(iter->key_value->first, key)) {
                return iter;
            }
            ++iter;
        }
        return end();
    }

    Iterator erase(ConstIterator iter) {
        size_t index = iter.position_->hash % iterators_.size();
        traits_t::destroy(alloc_, iter.position_->key_value);
        traits_t::deallocate(alloc_, iter.position_->key_value, 1);
        iter.position_->key_value = nullptr;
        if (iterators_[index] == iter.position_) {
            iterators_[index] = elements_.erase(iter.position_);
            ListTypeIter next = iterators_[index];
            if (next != elements_.end()
                && next->hash % iterators_.size() != index) {
                iterators_[index] = elements_.end();
            }
            return next;
        }
        return elements_.erase(iter.position_);
    }
    
    Iterator erase(ConstIterator first, ConstIterator last) {
        while (first != last) {
            first = erase(first);
        }
        return first.position_;
    }

    void rehash(size_t count) {
        count = std::max(count, iterators_.size());
        iterators_.clear();
        iterators_.resize(count, elements_.end());
        ListTypeIter current = elements_.begin();
        while (current != elements_.end()) {
            ListTypeIter next = ++current;
            --current;
            size_t index = current->hash % count;
            elements_.splice(
                iterators_[index] == elements_.end() ?
                elements_.begin() : iterators_[index],
                current
            );
            iterators_[index] = current;
            current = next;
        }
    }

    template <typename... Args>
    std::pair<Iterator, bool> emplace(Args&&... args) {
        NodeType* node = traits_t::allocate(alloc_, 1);
        try {
            traits_t::construct(alloc_, node, std::forward<Args>(args)...);
        } catch (...) {
            traits_t::deallocate(alloc_, node, 1);
            throw;
        }
        size_t hash;
        try {
            hash = hash_(node->first);
        } catch (...) {
            traits_t::destroy(alloc_, node);
            traits_t::deallocate(alloc_, node, 1);
            throw;
        }
        Iterator iter = find(node->first);
        if (iter != end()) {
            traits_t::destroy(alloc_, node);
            traits_t::deallocate(alloc_, node, 1);
            return {iter, false};
        }
        size_t index = hash % iterators_.size();
        iterators_[index] = elements_.insert(
            iterators_[index] == elements_.end() ?
            elements_.begin() : iterators_[index],
            ListNode(node, hash)
        );
        iter = iterators_[index];
        if (load_factor() > max_load_factor_) {
            rehash(iterators_.size() * 2);
        }
        return {iter, true};
    }

    Value& operator[](const Key& key) {
        return emplace(key, Value()).first->second;
    }

    Value& operator[](Key&& key) {
        return emplace(std::forward<Key>(key), Value()).first->second;
    }

    Value& at(const Key& key) {
        Iterator iter = find(key);
        if (iter == end()) {
            throw std::out_of_range("Out of range");
        }
        return iter->second;
    }

    const Value& at(const Key& key) const {
        return const_cast<const UnorderedMap>(*this).at(key);
    }

    size_t size() const {
        return elements_.size();
    }

    std::pair<Iterator, bool> insert(const NodeType& node) {
        return emplace(node);
    }

    std::pair<Iterator, bool> insert(NodeType&& node) {
        return emplace(std::move(const_cast<Key&>(node.first)), std::move(node.second));
    }

    template <typename InputIter>
    void insert(InputIter first, InputIter last) {
        while (first != last) {
            insert(*first++);
        }
    }

    void reserve(size_t count) {
        rehash(count);
    }

    size_t max_size() const {
        return 100000000;
    }

    double load_factor() const {
        return static_cast<double>(elements_.size()) /
               static_cast<double>(iterators_.size());
    }

    double max_load_factor() const {
        return max_load_factor_;
    }
};


//#include <iostream>
//#include <string>

//int main() {
    
//}

