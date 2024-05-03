#include <iostream>
#include <vector>
#include <string>
#include <cmath>

class Complex {
private:
    double real_, imag_;

public:
    Complex() : real_(0), imag_(0) {}

    Complex(double real, double imag) : real_(real), imag_(imag) {}

    Complex(int real) : real_(real), imag_(0) {}

    Complex(const Complex& number) = default;

    ~Complex() = default;

    Complex& operator=(const Complex& number) = default;

    Complex& operator+=(const Complex& number) {
        real_ += number.real_;
        imag_ += number.imag_;
        return *this;
    }

    Complex& operator-=(const Complex& number) {
        real_ -= number.real_;
        imag_ -= number.imag_;
        return *this;
    }

    Complex& operator*=(const Complex& number) {
        double old_real = real_;
        real_ = old_real * number.real_ - imag_ * number.imag_;
        imag_ = old_real * number.imag_ + imag_ * number.real_;
        return *this;
    }

    double real() {
        return real_;
    }

    double imag() {
        return imag_;
    }
};

Complex operator+(Complex left, const Complex& right) {
    return left += right;
}

Complex operator-(Complex left, const Complex& right) {
    return left -= right;
}

Complex operator*(Complex left, const Complex& right) {
    return left *= right;
}

class BigInteger {
private:
    std::vector<int> buffer_;
    int sign_;
    static const int ten_power_ = 2;
    static const int base_ = 100;
    static constexpr double pi_ = acos(0) * 2;
    static std::vector<Complex> polynomial1_;
    static std::vector<Complex> polynomial2_;

    void flush() {
        buffer_.resize(1);
        buffer_[0] = 0;
        sign_ = 1;
    }

    void normalize() {
        while (buffer_.size() > 1 && buffer_.back() == 0) {
            buffer_.pop_back();
        }
        if (buffer_.size() == 1 && buffer_[0] == 0)
            sign_ = 1;
    }

    void fft(std::vector<Complex>& source, int size, int inverse) {
        int log_n = __builtin_ctz(size);
        for (int i = 0; i < size; ++i) {
            int reversed = 0;
            for (int j = 0; j < log_n; ++j) {
                reversed |= ((i >> j) & 1) << (log_n - j - 1);
            }
            if (i < reversed)
                std::swap(source[i], source[reversed]);
        }
        for (int length = 2; length <= size; length *= 2) {
            double angle = pi_ * 2 / length;
            Complex root(cos(angle), inverse * sin(angle));
            for (int i = 0; i < size; i += length) {
                Complex power = 1;
                int new_length = length / 2;
                for (int j = 0; j < new_length; ++j) {
                    Complex left = source[i + j];
                    Complex right = source[i + j + new_length] * power;
                    source[i + j] = left + right;
                    source[i + j + new_length] = left - right;
                    power *= root;
                }
            }
        }
    }

    bool less_abs(const BigInteger& number) const {
        if (*this == number)
            return false;
        if (buffer_.size() != number.buffer_.size())
            return buffer_.size() < number.buffer_.size();
        for (int i = buffer_.size() - 1; i >= 0; --i) {
            if (buffer_[i] != number.buffer_[i])
                return buffer_[i] < number.buffer_[i];
        }
        return false;
    }

    void add_abs(const BigInteger& number) {
        int carry = 0;
        for (size_t i = 0; i < number.buffer_.size() || carry > 0; ++i) {
            if (i == buffer_.size())
                buffer_.push_back(0);
            int add = i < number.buffer_.size() ? number.buffer_[i] : 0;
            int sum = buffer_[i] + add + carry;
            buffer_[i] = sum % base_;
            carry = sum / base_;
        }
        normalize();
    }

    void sub_abs(const BigInteger& number) {
        if (less_abs(number)) {
            int carry = 0;
            for (size_t i = 0; i < number.buffer_.size(); ++i) {
                int sub = i < buffer_.size() ? buffer_[i] : 0;
                if (i == buffer_.size())
                    buffer_.push_back(0);
                buffer_[i] = number.buffer_[i] - sub - carry;
                carry = buffer_[i] < 0 ? 1 : 0;
                if (carry > 0)
                    buffer_[i] += base_;
            }
        } else {
            int carry = 0;
            for (size_t i = 0; i < number.buffer_.size() || carry > 0; ++i) {
                int sub = i < number.buffer_.size() ? number.buffer_[i] : 0;
                buffer_[i] -= sub + carry;
                carry = buffer_[i] < 0 ? 1 : 0;
                if (carry > 0)
                    buffer_[i] += base_;
            }
        }
        normalize();
    }

    BigInteger multiply_short(int number) const {
        BigInteger result(*this);
        int carry = 0;
        for (size_t i = 0; i < result.buffer_.size(); ++i) {
            int product = result.buffer_[i] * number + carry;
            result.buffer_[i] = product % base_;
            carry = product / base_;
        }
        while (carry > 0) {
            result.buffer_.push_back(carry % base_);
            carry /= base_;
        }
        result.normalize();
        return result;
    }

    void shift() {
        if (buffer_.size() == 1 && buffer_[0] == 0)
            return;
        buffer_.push_back(0);
        for (int i = buffer_.size() - 2; i >= 0; --i) {
            buffer_[i + 1] = buffer_[i];
        }
        buffer_[0] = 0;
    }

public:
    BigInteger() : buffer_(1, 0), sign_(1) {}

    BigInteger(int number) : buffer_(), sign_(number >= 0 ? 1 : -1) {
        number = abs(number);
        while (number > 0) {
            buffer_.push_back(number % base_);
            number /= base_;
        }
        if (buffer_.empty())
            buffer_.push_back(0);
    }

    BigInteger(const BigInteger& number) = default;

    BigInteger& operator=(const BigInteger& number) = default;

    ~BigInteger() = default;

    explicit operator bool() const {
        return *this != 0;
    }

    BigInteger operator+() const {
        return BigInteger(*this);
    }

    BigInteger operator-() const {
        BigInteger result(*this);
        result.sign_ *= -1;
        result.normalize();
        return result;
    }

    bool operator==(const BigInteger& number) const {
        if (sign_ != number.sign_ || buffer_.size() != number.buffer_.size())
            return false;
        for (size_t i = 0; i < buffer_.size(); ++i) {
            if (buffer_[i] != number.buffer_[i])
                return false;
        }
        return true;
    }

    bool operator<(const BigInteger& number) const {
        if (*this == number)
            return false;
        if (sign_ == -1 && number.sign_ == -1)
            return -number < -*this;
        if (sign_ == -1)
            return true;
        if (number.sign_ == -1)
            return false;
        return less_abs(number);
    }

    bool operator!=(const BigInteger& number) const {
        return !operator==(number);
    }

    bool operator<=(const BigInteger& number) const {
        return operator<(number) || operator==(number);
    }

    bool operator>=(const BigInteger& number) const {
        return !operator<(number);
    }

    bool operator>(const BigInteger& number) const {
        return !operator<=(number);
    }

    BigInteger& operator+=(const BigInteger& number) {
        if (sign_ == number.sign_) {
            add_abs(number);
        } else {
            if (less_abs(number))
                sign_ *= -1;
            sub_abs(number);
        }
        normalize();
        return *this;
    }

    BigInteger& operator-=(const BigInteger& number) {
        if (sign_ != number.sign_) {
            add_abs(number);
        } else {
            if (less_abs(number))
                sign_ *= -1;
            sub_abs(number);
        }
        normalize();
        return *this;
    }

    BigInteger& operator*=(const BigInteger& number) {
        size_t max_size = std::max(buffer_.size(), number.buffer_.size());
        size_t size = 1;
        while (size < max_size) {
            size *= 2;
        }
        size *= 2;
        polynomial1_.resize(size);
        polynomial2_.resize(size);
        std::fill(polynomial1_.begin(), polynomial1_.end(), 0);
        std::fill(polynomial2_.begin(), polynomial2_.end(), 0);
        for (size_t i = 0; i < buffer_.size(); ++i) {
            polynomial1_[i] = buffer_[i];
        }
        for (size_t i = 0; i < number.buffer_.size(); ++i) {
            polynomial2_[i] = number.buffer_[i];
        }
        fft(polynomial1_, size, 1);
        fft(polynomial2_, size, 1);
        for (size_t i = 0; i < size; ++i) {
            polynomial1_[i] *= polynomial2_[i];
        }
        fft(polynomial1_, size, -1);
        unsigned long long carry = 0;
        for (size_t i = 0; i < size; ++i) {
            if (i == buffer_.size())
                buffer_.push_back(0);
            unsigned long long sum = llround(polynomial1_[i].real() / static_cast<double>(size)) + carry;
            buffer_[i] = sum % base_;
            carry = sum / base_;
        }
        while (carry > 0) {
            buffer_.push_back(carry % base_);
            carry /= base_;
        }
        sign_ *= number.sign_;
        normalize();
        return *this;
    }

    BigInteger& operator/=(const BigInteger& number) {
        BigInteger current;
        for (int i = buffer_.size() - 1; i >= 0; --i) {
            current.shift();
            current.buffer_[0] = buffer_[i];
            int left = 0, right = base_ - 1;
            while (left < right) {
                int middle = (left + right) / 2;
                if ((number.multiply_short(middle)).less_abs(current))
                    left = middle + 1;
                else
                    right = middle;
            }
            if (current.less_abs(number.multiply_short(left)))
                --left;
            buffer_[i] = left;
            current.sub_abs(number.multiply_short(left));
        }
        sign_ *= number.sign_;
        normalize();
        return *this;
    }

    BigInteger& operator%=(const BigInteger& number);

    BigInteger& operator++() {
        return *this += 1;
    }

    BigInteger operator++(int) {
        BigInteger result(*this);
        *this += 1;
        return result;
    }

    BigInteger& operator--() {
        return *this -= 1;
    }

    BigInteger operator--(int) {
        BigInteger result(*this);
        *this -= 1;
        return result;
    }

    std::string toString() const {
        std::string result = sign_ == -1 ? "-" : "";
        for (int i = buffer_.size() - 1; i >= 0; --i) {
            std::string digit = std::to_string(buffer_[i]);
            if (static_cast<int>(digit.size()) < ten_power_ &&
                i < static_cast<int>(buffer_.size()) - 1)
                digit = std::string(ten_power_ - digit.size(), '0') + digit;
            result += digit;
        }
        return result;
    }

    void swap(BigInteger& number) {
        buffer_.swap(number.buffer_);
        std::swap(sign_, number.sign_);
    }

    friend std::istream& operator>>(std::istream& input, BigInteger& number);

    friend std::ostream& operator<<(std::ostream& output, const BigInteger& number);    
};

std::vector<Complex> BigInteger::polynomial1_;
std::vector<Complex> BigInteger::polynomial2_;

BigInteger operator+(BigInteger left, const BigInteger& right) {
    return left += right;
}

BigInteger operator-(BigInteger left, const BigInteger& right) {
    return left -= right;
}

BigInteger operator*(BigInteger left, const BigInteger& right) {
    return left *= right;
}

BigInteger operator/(BigInteger left, const BigInteger& right) {
    return left /= right;
}

BigInteger operator%(BigInteger left, const BigInteger& right) {
    return left %= right;
}

BigInteger& BigInteger::operator%=(const BigInteger& number) {
    *this -= (BigInteger(*this) /= number) * number;
    normalize();
    return *this;
}

std::istream& operator>>(std::istream& input, BigInteger& number) {
    std::string raw_number;
    input >> raw_number;
    number.flush();
    if (raw_number[0] == '-') {
        number.sign_ = -1;
        raw_number = raw_number.substr(1);
    }
    number.buffer_.clear();
    for (int i = raw_number.size(); i > 0; i -= BigInteger::ten_power_) {
        int left = std::max(0, i - BigInteger::ten_power_);
        number.buffer_.push_back(std::stoi(raw_number.substr(left, i - left)));
    }
    return input;
}

std::ostream& operator<<(std::ostream& output, const BigInteger& number) {
    output << number.toString();
    return output;
}

BigInteger abs(const BigInteger& number) {
    return number < 0 ? -number : number;
}

BigInteger gcd(BigInteger left, BigInteger right) {
    while (right != 0) {
        left.swap(right);
        right %= left;
    }
    return left;
}

class Rational {
private:
    BigInteger numerator_;
    BigInteger denominator_;

    void normalize() {
        BigInteger common_divisor = gcd(abs(numerator_), denominator_);
        numerator_ /= common_divisor;
        denominator_ /= common_divisor;
    }

public:
    Rational() : numerator_(0), denominator_(1) {}

    Rational(const BigInteger& number) : numerator_(number), denominator_(1) {}

    Rational(int number) : numerator_(number), denominator_(1) {}

    Rational(const Rational& number) = default;

    ~Rational() = default;

    Rational operator+() const {
        return Rational(*this);
    }

    Rational operator-() const {
        Rational result(*this);
        result.numerator_ = -result.numerator_;
        return result;
    }

    bool operator==(const Rational& number) const {
        return numerator_ == number.numerator_ && denominator_ == number.denominator_;
    }

    bool operator<(const Rational& number) const {
        return numerator_ * number.denominator_ < number.numerator_ * denominator_;
    }

    bool operator!=(const Rational& number) const {
        return !operator==(number);
    }

    bool operator<=(const Rational& number) const {
        return operator==(number) || operator<(number);
    }

    bool operator>(const Rational& number) const {
        return !operator<=(number);
    }

    bool operator>=(const Rational& number) const {
        return !operator<(number);
    }

    Rational& operator+=(const Rational& number) {
        BigInteger common_divisor = gcd(denominator_, number.denominator_);
        BigInteger common_multiple = denominator_ * number.denominator_ / common_divisor;
        numerator_ = numerator_ * common_multiple / denominator_ +
                     number.numerator_ * common_multiple / number.denominator_;
        denominator_ = common_multiple;
        normalize();
        return *this;
    }

    Rational& operator-=(const Rational& number) {
        return *this += -number;
    }

    Rational& operator*=(const Rational& number) {
        numerator_ *= number.numerator_;
        denominator_ *= number.denominator_;
        normalize();
        return *this;
    }

    Rational& operator/=(const Rational& number) {
        numerator_ *= number.denominator_;
        denominator_ *= number.numerator_;
        if (denominator_ < 0) {
            denominator_ = -denominator_;
            numerator_ = -numerator_;
        }
        normalize();
        return *this;
    }

    std::string toString() const  {
        if (denominator_ == BigInteger(1))
            return numerator_.toString();
        return numerator_.toString() + '/' + denominator_.toString();
    }

    explicit operator double() const {
        return std::stod(asDecimal(30));
    }

    std::string asDecimal(size_t precision = 0) const {
        BigInteger number = abs(numerator_);
        for (size_t i = 0; i < precision; ++i)
            number *= 10;
        number /= denominator_;
        std::string result = number.toString();
        result = std::string(std::max(0, static_cast<int>(precision) -
                                         static_cast<int>(result.size())), '0') +
                 result;
        if (precision > 0) {
            int position = std::max(0, static_cast<int>(result.size()) -
                                       static_cast<int>(precision));
            if (position == 0)
                result = "0." + result;
            else
                result = result.substr(0, position) + '.' + result.substr(position);
        }
        if (numerator_ < 0)
            return '-' + result;
        return result;
    }
};

Rational operator+(Rational left, const Rational& right) {
    return left += right;
}

Rational operator-(Rational left, const Rational& right) {
    return left -= right;
}

Rational operator*(Rational left, const Rational& right) {
    return left *= right;
}

Rational operator/(Rational left, const Rational& right) {
    return left /= right;
}
