#pragma once

#include "std.h"

struct Users {
	struct User {
		fixed_string<20>::cstr name;
		size_t idx = 0;
	};
	inplace_vector<User, 6> users;

	std::optional<size_t> Add(const fixed_string<20> &name);
	optional_ref<User> Get(size_t id);
	size_t size() const;
};

extern Users users;
