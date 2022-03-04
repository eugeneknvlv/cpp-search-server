#include <algorithm>
#include <numeric>
#include <cmath>
#include "search_server.h"
#include "string_processing.h"
#include "document.h"
#include "log_duration.h"

using namespace std;

SearchServer::SearchServer(const string& stop_words_string) 
    : SearchServer(SplitIntoWords(stop_words_string))
{}



void SearchServer::AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& marks) {
    if (document_id < 0) {
        throw invalid_argument("document id is less than zero");
    }
    if (documents_.count(document_id) > 0) {
        throw invalid_argument("there is already a document with this id");
    }
    vector<string> words = SplitIntoWordsNoStop(document);       
    const double inv_word_count = 1.0 / words.size();           
    for (const string& word : words) {
        word_to_documents_freqs_[word][document_id] += inv_word_count;  // calculating Term Frequency
        document_id_to_word_freqs_[document_id][word] += inv_word_count;  
        documents_content_[document_id].insert(word);       
    }
    int rating = ComputeAverageRating(marks);
    DocumentProperties properties = {rating, status};
    documents_.emplace(document_id, properties);
    document_ids_.push_back(document_id);
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query, DocumentStatus doc_status) const {
    return FindTopDocuments(raw_query, 
                            [doc_status](int document_id, DocumentStatus status, int rating)
                            { return status == doc_status; } 
    );
}



std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}



tuple<vector<string>, DocumentStatus> SearchServer::MatchDocument(const string& raw_query, int document_id) const {
    LOG_DURATION_STREAM("Operation time"s, cerr);
    Query query = ParseQuery(raw_query);

    vector<string> matched_words;
    for (const string& word : query.plus_words) {
        if (word_to_documents_freqs_.count(word)) {
            if (word_to_documents_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
    }

    for (const string& word : query.minus_words) {
        if (word_to_documents_freqs_.count(word)) {
            if (word_to_documents_freqs_.at(word).count(document_id)) {
                matched_words.clear();
                break;
            }
        }
    }
    
    sort(matched_words.begin(), matched_words.end());
    return tuple<vector<string>, DocumentStatus>(matched_words, documents_.at(document_id).status);
}



int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

std::vector<int>::const_iterator SearchServer::begin() const {
    return document_ids_.begin();
}

std::vector<int>::const_iterator SearchServer::end() const {
    return document_ids_.end();
}


const std::map<std::string, double>& SearchServer::GetWordFrequencies(int document_id) const {
    static const map<string, double> empty_map = {};
    if(!binary_search(document_ids_.begin(), document_ids_.end(), document_id)) {
        return empty_map;
    }
    return document_id_to_word_freqs_.at(document_id);
}


void SearchServer::RemoveDocument(int document_id) {

    document_id_to_word_freqs_.erase(document_id);

    auto pos = find(document_ids_.begin(), document_ids_.end(), document_id);
    document_ids_.erase(pos);

    documents_.erase(document_id);

    for (auto& [word, doc_ids_to_TFs] : word_to_documents_freqs_) {
        doc_ids_to_TFs.erase(document_id);
    }

    documents_content_.erase(document_id);
}


vector<int> SearchServer::FindDuplicates() {
    set< set<string> > already_seen_docs;
    vector<int> ids_to_erase;
    for (const auto& [id, content] : documents_content_) {
        if (count(already_seen_docs.begin(), already_seen_docs.end(), content)) {
            ids_to_erase.push_back(id);
        }
        else {
            already_seen_docs.insert(content);
        }
    }
    return ids_to_erase;
}


bool SearchServer::IsStopWord(const std::string& word) const {
    return stop_words_.count(word);
}



bool SearchServer::IsValidWord(const std::string& word) {
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}



vector<string> SearchServer::SplitIntoWordsNoStop(const std::string& text) const {
    vector<string> words;
    for (const string& word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw invalid_argument("document contains invalid symbols");
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        } 
    }
    return words;
}



SearchServer::Query SearchServer::ParseQuery(const string& query_string) const {
    Query query;
    for (const string& word : SplitIntoWords(query_string)) {
        if (!IsValidWord(word)) {
            throw invalid_argument("document contains invalid symbols");
        }

        if (word[0] == '-') {
            const string minus_word = word.substr(1);
            if (minus_word.empty()) {
                throw invalid_argument("empty minus-word");
            }
            if (minus_word[0] == '-') {
                throw invalid_argument("two minuses in a row");
            }
            query.minus_words.insert(word.substr(1));
            continue;
        }

        query.plus_words.insert(word);
    }
    return query;
}



double SearchServer::CalcIDF(const string& word) const {
    return log( (1.0 * SearchServer::GetDocumentCount()) / word_to_documents_freqs_.at(word).size() );
}



int SearchServer::ComputeAverageRating(const vector<int>& marks) {
    int size = marks.size();
    int sum = accumulate(marks.begin(), marks.end(), 0);
    return sum / size;
}