#include "user.h"

Users users;

std::optional<int> Users::Add(char name[20]) {
	for (int i = 0; i < users.size(); i++) {
		if (strncmp(users.try_at(i)->name, name, 20) == 0) {
			return i;
		}
	}
	if (users.full()) return {};
	int idx = users.size();
	User &user = *users.try_emplace_back();
	memcpy(user.name, name, 20);
	user.idx = idx;
	return idx;
}

optional_ref<Users::User> Users::Get(int id) {
	return users.try_at(id);
}

inline size_t Users::size() const {
	return users.size();
}
