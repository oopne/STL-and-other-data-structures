#include <iostream>
#include <cstring>

class String {
private:
    char* buffer_;
    int capacity_;
    int size_;
    
    static const char end_of_string = '\0';

    bool are_equal(int i, int j, int separator) {
        return buffer_[i] == buffer_[j]
               && !(i == separator && j != separator)
               && !(i != separator && j == separator);
    }

    void extend(int new_capacity) {
        capacity_ = new_capacity;
        buffer_ = reinterpret_cast<char*>(realloc(buffer_, capacity_));
    }

    void pi_function(int*& pi, int separator) {
        pi = reinterpret_cast<int*>(malloc(capacity_ * sizeof(int)));
        pi[0] = 0;
        for (int i = 1, j = 0; i < size_; ++i) {
            while (j > 0 && !are_equal(i, j, separator)) {
                j = pi[j - 1];
            }
            if (are_equal(i, j, separator))
                ++j;
            pi[i] = j;
        }
    }
    
    int knuth_morris_pratt(const String& substring, bool reverse) const;
    
public:
    String() : capacity_(1), size_(0) {
        buffer_ = reinterpret_cast<char*>(malloc(capacity_));
        buffer_[0] = end_of_string;
    }
    
    String(int count, char character) : capacity_(count + 1), size_(count) {
        buffer_ = reinterpret_cast<char*>(malloc(capacity_));
        memset(buffer_, character, size_);
        buffer_[size_] = end_of_string;
    }
    
    String(const char* init_buffer) : capacity_(strlen(init_buffer) + 1),
                                      size_(capacity_ - 1) {
        buffer_ = reinterpret_cast<char*>(malloc(capacity_));
        memcpy(buffer_, init_buffer, size_);
        buffer_[size_] = end_of_string;
    }
    
    String(char character) : capacity_(2), size_(1) {
        buffer_ = reinterpret_cast<char*>(malloc(capacity_));
        buffer_[0] = character;
        buffer_[size_] = end_of_string;
    }
    
    String(const String& source) : capacity_(source.capacity_),
                                   size_(source.size_) {
        buffer_ = reinterpret_cast<char*>(malloc(capacity_));
        memcpy(buffer_, source.buffer_, size_);
        buffer_[size_] = end_of_string;
    }
    
    ~String() {
        free(reinterpret_cast<void*>(buffer_));
    }
    
    String& operator=(const String& source) & {
        capacity_ = source.capacity_;
        size_ = source.size_;
        buffer_ = reinterpret_cast<char*>(realloc(buffer_, capacity_));
        memcpy(buffer_, source.buffer_, size_);
        buffer_[size_] = end_of_string;
        return *this;
    }

    friend bool operator==(const String& string1, const String& string2);

    char& operator[](int position) {
        return buffer_[position];
    }

    const char& operator[](int position) const {
        return buffer_[position];
    }

    int length() const {
        return size_;
    }

    void push_back(char character) {
        if (size_ + 2 > capacity_)
            extend(capacity_ * 2);
        buffer_[size_++] = character;
        buffer_[size_] = end_of_string;
    }

    void pop_back() {
        buffer_[--size_] = end_of_string;
    }
    
    char& front() {
        return buffer_[0];
    }
    
    const char& front() const {
        return buffer_[0];
    }
    
    char& back() {
        return buffer_[size_ - 1];
    }
    
    const char& back() const {
        return buffer_[size_ - 1];
    }
    
    String& operator+=(char character) {
        push_back(character);
        return *this;
    }
    
    String& operator+=(const String& add_string) {
        int new_size = size_ + add_string.size_;
        if (new_size + 1 > capacity_) {
            int new_capacity = capacity_;
            while (new_capacity < new_size + 1)
                new_capacity *= 2;
            extend(new_capacity);
        }
        memcpy(buffer_ + size_, add_string.buffer_, add_string.size_);
        size_ += add_string.size_;
        buffer_[size_] = end_of_string;
        return *this;
    }
    
    unsigned find(const String& substring) const;
    
    unsigned rfind(const String& substring) const;
    
    String substr(int start, int count) const {
        char tmp = buffer_[start + count];
        buffer_[start + count] = end_of_string;
        String substring = String(buffer_ + start);
        buffer_[start + count] = tmp;
        return substring;
    }
    
    bool empty() {
        return size_ == 0;
    }
    
    void clear() {
        buffer_ = reinterpret_cast<char*>(realloc(buffer_, 1));
        buffer_[0] = end_of_string;
        capacity_ = 1;
        size_ = 0;
    }
    
    friend std::istream& operator>>(std::istream& input, String& string);
    
    friend std::ostream& operator<<(std::ostream& output, const String& string);
};

bool operator==(const String& string1, const String& string2) {
    return strcmp(string1.buffer_, string2.buffer_) == 0;
}

String operator+(const String& string1, const String& string2) {
    return String(string1) += string2;
}

int String::knuth_morris_pratt(const String& substring, bool reverse) const {
    int* pi;
    const char separator = '#';
    (substring + separator + *this).pi_function(pi, substring.size_);
    int total_size = substring.size_ + size_ + 1;
    int result = size_;
    for (int i = substring.size_ + 1; i < total_size; ++i) {
        if (pi[i] == substring.size_) {
            if (!reverse) {
                free(reinterpret_cast<void*>(pi));
                return i - substring.size_ * 2;
            } else {
                result = i - substring.size_ * 2;
            }
        }
    }
    free(reinterpret_cast<void*>(pi));
    return result;
}

unsigned String::find(const String& substring) const {
    return knuth_morris_pratt(substring, false);
}

unsigned String::rfind(const String& substring) const {
    return knuth_morris_pratt(substring, true);
}

std::istream& operator>>(std::istream& input, String& string) {
    char character;
    do {
        input.get(character);
    } while (!input.eof() && isspace(character));
    string.clear();
    while (!input.eof() && !isspace(character)) {
        string.push_back(character);
        input.get(character);
    }
    return input;
}

std::ostream& operator<<(std::ostream& output, const String& string) {
    output << string.buffer_;
    return output;
}

int main() {
	String s = "12131";
	"std::cout << 'a' + s;" == s;
}
