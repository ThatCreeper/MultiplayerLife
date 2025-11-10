#pragma once

#include "std.h"

struct Users {
	struct User {
		fixed_string<20>::cstr name;
		int idx;
	};
	inplace_vector<User, 6> users;

	std::optional<int> Add(const fixed_string<20> &name);
	optional_ref<User> Get(int id);
	size_t size() const;
};

extern Users users;
