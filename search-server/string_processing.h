#pragma once
#include <string>
#include <vector>
#include <set>
#include <string_view>

std::vector<std::string_view> SplitIntoWordsView(const std::string_view text_sv);

template <typename StringContainer>
std::set<std::string, std::less<>> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
	std::set<std::string, std::less<>> non_empty_strings;
	for (std::string_view str : strings) {
		if (!str.empty()) {
			non_empty_strings.emplace(str);
		}
	}
	return non_empty_strings;
}