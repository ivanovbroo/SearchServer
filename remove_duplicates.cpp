#include <algorithm>
#include <set>
#include <iostream>

#include "remove_duplicates.h"

using namespace std;

void RemoveDuplicates(SearchServer& search_server) {
    set<int> duplicates;

    for (auto i = search_server.begin(); i != search_server.end(); ++i) {
        for (auto j = next(i, 1); j != search_server.end(); ++j) {
            const map<string_view, double>& words_from_first_document = search_server.GetWordFrequencies(*i);
            const map<string_view, double>& words_from_second_document = search_server.GetWordFrequencies(*j);

            if (words_from_first_document.size() != words_from_second_document.size()) {
                continue;
            }

            const bool is_equal = equal(words_from_first_document.begin(), words_from_first_document.end(),
                words_from_second_document.begin(),
                [](const auto& a, const auto& b) {
                    return a.first == b.first; 
                });

            if (is_equal) {
                duplicates.insert(*j);
            }
        }
    }

    for (int i : duplicates) {
        cout << "Found duplicate document id "s << i << endl;
        search_server.RemoveDocument(i);
    }
}