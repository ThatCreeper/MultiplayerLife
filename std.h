#pragma once

// Some helper classes which are mostly based off of ones
// in C++26
// Also all of the standard includes

#undef NDEBUG
#include <cassert>
#include <raylib.h>
#include <raymath.h>
#include <cstring>
#include <optional>
#include <span>
#include <algorithm>
#include <vector>
#include <array>
#include <format>
#include <print>
#include <string_view>
#define strncpy strncpy_s

// T* with some extra safety
template <class T>
struct optional_ref {
	T *val;
	optional_ref() : val(nullptr) {}
	optional_ref(T &v) : val(&v) {}
	optional_ref(optional_ref<T> &v) : val(v.val) {}

	T &value() {
		assert(val);
		return *val;
	}
	T *operator ->() {
		assert(val);
		return val;
	}
	T &operator *() {
		return value();
	}
	operator bool() const {
		return val;
	}

	void reset() {
		val = nullptr;
	}
	T &emplace(T &v) {
		val = &v;
		return v;
	}
};

template <class T, size_t N>
struct inplace_vector {
	union {
		T values[N];
		int _dummy;
	};
	size_t count = 0;

	inplace_vector() : _dummy(0) {}
	~inplace_vector() {
		for (T &t : *this) {
			t.~T();
		}
	}
	inplace_vector(const inplace_vector<T, N> &other) = delete;
	inplace_vector(inplace_vector<T, N> &&other) = delete;
	inplace_vector<T, N> &operator =(const inplace_vector<T, N> &other) = delete;
	inplace_vector<T, N> &operator =(inplace_vector<T, N> &&other) = delete;

	const T *begin() const {
		return values;
	}
	const T *end() const {
		return values + count;
	}
	T *begin() {
		return values;
	}
	T *end() {
		return values + count;
	}

	size_t size() const {
		return count;
	}
	size_t capacity() const {
		return N;
	}

	template<class... Args>
	optional_ref<T> try_emplace_back(Args&&... args) {
		if (full()) return {};
		T *out = &values[count++];
		new(out) T(std::forward(args)...);
		return *out;
	}

	optional_ref<T> try_push_back(const T &val) {
		if (full()) return {};
		T *out = &values[count++];
		new(out) T(val);
		return *out;
	}

	bool full() const {
		return count >= N;
	}

	optional_ref<const T> try_at(size_t pos) const {
		if (pos >= count) return {};
		return values[pos];
	}
	optional_ref<T> try_at(size_t pos) {
		if (pos >= count) return {};
		return values[pos];
	}
};

template <class T, size_t N>
struct reusable_inplace_vector {
	using parent = inplace_vector<std::optional<T>, N>;
	parent store;
	int reusables = 0;

	bool full() {
		return store.count >= N && reusables <= 0;
	}

	struct iterator {
		std::optional<T> *p;
		reusable_inplace_vector<T, N> &v;
		T *operator ->() {
			return &**p;
		}
		T &operator *() {
			return **p;
		}
		iterator &operator ++() {
			std::optional<T> *end = v.end().p;
			while (p != end) {
				p++;
				if (*p) return *this;
			}
			return *this;
		}
		bool operator==(const iterator &other) {
			return p == other.p;
		}
		bool operator!=(const iterator &other) {
			return p != other.p;
		}
	};

	iterator begin() {
		iterator it{ .p = store.begin(), .v = *this };
		iterator ie = end();
		while (it != ie) {
			if (*(it.p)) return it;
			++it;
		}
		return ie;
	}

	iterator end() {
		return { .p = store.end(), .v = *this };
	}

	template<class... Args>
	optional_ref<T> try_emplace_replace(Args&&... args) {
		if (full()) return {};
		if (reusables > 0) {
			for (std::optional<T> &member : store) {
				if (member) continue;
				member.emplace(std::forward(args)...);
				reusables--;
				return *member;
			}
		}
		optional_ref<std::optional<T>> out = store.try_emplace_back();
		(*out).emplace(std::forward(args)...);
		return **out;
	}

	optional_ref<T> try_push_replace(const T &val) {
		if (full()) return {};
		if (reusables > 0) {
			for (std::optional<T> &member : store) {
				if (member) continue;
				member = val;
				reusables--;
				return *member;
			}
		}
		optional_ref<std::optional<T>> out = store.try_emplace_back();
		(*out) = val;
		return **out;
	}

	optional_ref<T> replace_push_replace(const T &val) {
		optional_ref<T> r1 = try_push_replace(val);
		if (r1) return r1;
		// If we're full, pick a random value and replace it
		optional_ref<std::optional<T>> out = store.try_at(GetRandomValue(0, N - 1));
		(*out) = val;
		return **out;
	}

	std::optional<size_t> index(T &member) {
		// this is kinda stupid but i don't want to do
		// anything smarter so take that
		for (int i = 0; i < store.size(); i++) {
			std::optional<T> &val = store.values[i];
			if (!val) continue;
			if (&*val == &member) {
				return i;
			}
		}
		return {};
	}

	optional_ref<T> try_at(size_t pos) {
		optional_ref<std::optional<T>> res = store.try_at(pos);
		if (!res) return {};
		return **res;
	}

	void remove_safe_iter(T &value) {
		// this is also kinda stupid but i don't want to do
		// anything smarter so take that
		for (std::optional<T> &member : store) {
			if (&*member == &value) {
				member.reset();
				reusables++;
				return;
			}
		}
	}

	void remove_if(auto f) {
		for (std::optional<T> &member : store) {
			if (member && f(*member)) {
				member.reset();
				reusables++;
			}
		}
	}
};

template <size_t N>
struct fixed_string {
public:
	using cstr = fixed_string<N + 1>;
	using shorter = fixed_string<N - 1>;

	char buf[N];

	fixed_string() {
		clear();
	}
	template <size_t N1>
		requires (N1 <= N)
	fixed_string(const char(&s)[N1]) {
		clear();
		std::copy(s, s + N1, buf);
	}
	explicit fixed_string(const cstr &s) {
		clear();
		copy_from(s);
	}
	fixed_string(const fixed_string<N> &) = delete;
	fixed_string(fixed_string<N> &&) = delete;
	fixed_string<N> &operator =(const fixed_string<N> &) = delete;
	fixed_string<N> &operator =(fixed_string<N> &&) = delete;

	std::span<char, N> data() {
		return buf;
	}
	std::span<const char, N> data() const {
		return buf;
	}
	const char *bytes() const {
		return buf;
	}

	template <size_t N1>
		requires(N1 <= N)
	void copy_from(const fixed_string<N1> &s) {
		std::copy(s.bytes(), s.bytes() + N1, buf);
	}
	void copy_from(const cstr &s) {
		assert(s[N] == '\0');
		std::copy(s.bytes(), s.bytes() + N, buf);
	}
	template <size_t N1>
		requires(N1 <= N)
	void copy_from(const char(&s)[N1]) {
		std::copy(s, s + N1, buf);
	}
	void copy_from(const char(&s)[N + 1]) {
		assert(s[N] == '\0');
		std::copy(s, s + N, buf);
	}

	char &operator[](size_t n) {
		assert(n >= 0 && n < N);
		return buf[n];
	}

	const char &operator[](size_t n) const {
		assert(n >= 0 && n < N);
		return buf[n];
	}

	size_t length() const {
		for (int i = 0; i < N; i++) {
			if (buf[i] == '\0') return i;
		}
		return N;
	}

	bool full() const {
		return length() == N;
	}

	void push_cback(char c) {
		size_t len = length();
		if (len >= N - 1) return;
		buf[len] = c;
		if (len < N - 2) buf[len + 1] = '\0';
	}

	void pop_cback() {
		size_t len = length();
		if (len == 0) return;
		buf[len - 1] = '\0';
	}

	void clear() {
		std::fill(buf, buf + N, 0);
	}

	template <size_t N1>
	bool equals(const fixed_string<N1> &other) const {
		if (length() != other.length()) return false;
		return strncmp(bytes(), other.bytes(), std::min(N, N1)) == 0;
	}

	// NOT REAL!
	const shorter &fake_ref_short() const {
		assert(buf[N - 1] == '\0');
		return *reinterpret_cast<const shorter *>(this);
	}

	std::string elongate() const {
		std::string out;
		out.resize(N, 0);
		std::copy(buf, buf + N, out.begin());
		return out;
	}

	operator std::string_view() const {
		assert(buf[N - 1] == '\0');
		return std::string_view(buf, N - 1);
	}
};

template<size_t N>
errno_t strncpy(const std::array<char, N> &dest, const char *src, size_t n) {
	return strncpy_s((char *)dest.data(), N, src, n);
}