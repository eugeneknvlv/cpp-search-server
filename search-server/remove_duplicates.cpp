#include "remove_duplicates.h"

#include <vector>
#include <iostream>

using namespace std;

void RemoveDuplicates(SearchServer& search_server) {
    vector<int> duplicates_ids = search_server.FindDuplicates();
    for (auto id : duplicates_ids) {
        cout << "Found duplicate document id "s << id << endl;
        search_server.RemoveDocument(id);
    }
}