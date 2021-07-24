#pragma once

#include <vector>		
#include <string>
#include <set>
#include <string_view>


#include "read_input_functions.h"

std::vector<std::string_view> SplitIntoWords(std::string_view text);

std::vector<std::string> SplitIntoWords2(const std::string& text);


using TransparentStringSet = std::set<std::string, std::less<>>;

template <typename StringContainer>
TransparentStringSet MakeUniqueNonEmptyStrings(const StringContainer& strings) {
    TransparentStringSet non_empty_strings;
    for (std::string_view str : strings) {
        if (!str.empty()) {
            non_empty_strings.emplace(str);
        }
    }
    return non_empty_strings;
}