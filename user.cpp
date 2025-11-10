#include "user.h"

Users users;

std::optional<size_t> Users::Add(const fixed_string<20> &name) {
	for (int i = 0; i < users.size(); i++) {
		if (users.try_at(i)->name.equals(name)) {
			return i;
		}
	}
	if (users.full()) return {};
	size_t idx = users.size();
	User &user = *users.try_emplace_back();
	user.name.copy_from(name);
	user.idx = idx;
	return idx;
}

optional_ref<Users::User> Users::Get(size_t id) {
	return users.try_at(id);
}

inline size_t Users::size() const {
	return users.size();
}
