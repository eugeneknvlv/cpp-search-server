#include "remove_duplicates.h"

#include <set>
#include <vector>
#include <iostream>

using namespace std;

void RemoveDuplicates(SearchServer& search_server) {
    set< set<string> > unique_documents_content;
    vector<int> duplicates_to_erase; /* container that saves duplicates ids to erase them after "range-based for" 
                                     finishes its job in order not to invalidate the iterator */
    for (const int document_id : search_server) {
        set<string> document_words_set;
        for (const auto& [word, TF] : search_server.GetWordFrequencies(document_id)) {
            document_words_set.insert(word);
        }
        
        if (unique_documents_content.count(document_words_set)) {
            // cout << "Found duplicate document id "s << document_id << endl; 
            // search_server.RemoveDocument(document_id); 
            duplicates_to_erase.push_back(document_id);
        }
        else {
            unique_documents_content.insert(document_words_set);
        }
    }

    for (const int id_to_erase : duplicates_to_erase) {
        cout << "Found duplicate document id "s << id_to_erase << endl;
        search_server.RemoveDocument(id_to_erase); 
    }
}