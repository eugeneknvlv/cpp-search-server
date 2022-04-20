#include "string_processing.h"
#include <iostream>
#include <algorithm>
#include <execution>
#include <sstream>
#include <string_view>
#include <vector>

using namespace std;

vector<string_view> SplitIntoWordsView(const string_view str) {
    vector<string_view> result;
    int64_t pos = 0;
    int64_t last_symb_pos = str.length();
    const int64_t pos_end = str.npos;
    while (true) {
        int64_t space = str.find(' ', pos);
        if (pos == last_symb_pos && space == pos_end) {
            break;
        }
        if (space == pos) {
            pos++;
            continue;
        }
        result.push_back(space == pos_end ? str.substr(pos) : str.substr(pos, space - pos));
        if (space == pos_end) {
            break;
        }
        else {
            pos = space + 1;
        }
    }

    return result;
}
