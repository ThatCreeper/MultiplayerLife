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

	bool full() const {
		return count >= N;
	}

	optional_ref<const T> try_at(int pos) const {
		if (pos >= count) return {};
		return values[pos];
	}
	optional_ref<T> try_at(int pos) {
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

	optional_ref<T> try_at(int pos) {
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
};
