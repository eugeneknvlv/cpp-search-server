#include <algorithm>
#include <numeric>
#include <cmath>
#include <execution>
#include <map>
#include <utility>
#include <string>
#include <string_view>
#include <chrono>
#include <thread>
#include "search_server.h"
#include "document.h"
#include "log_duration.h"


using namespace std;

SearchServer::SearchServer(const string& stop_words_string)
    : SearchServer(string_view(stop_words_string))
{}

SearchServer::SearchServer(string_view stop_words_string_view)
    : SearchServer(SplitIntoWordsView(stop_words_string_view))
{}



void SearchServer::AddDocument(int document_id, string_view document, DocumentStatus status, const vector<int>& marks) {
    if (document_id < 0 || documents_.count(document_id) > 0) {
        throw invalid_argument("invalind document id"s);
    }

    const auto [it, inserted] = documents_.emplace(document_id, DocumentData{ ComputeAverageRating(marks), status, string(document) });
    vector<string_view> words = SplitIntoWordsNoStop((it->second).doc_text);

    const double inv_word_count = 1.0 / words.size();
    for (const string_view word : words) {
        word_to_documents_freqs_[word][document_id] += inv_word_count;  // calculating Term Frequency
        document_id_to_word_freqs_[document_id][word] += inv_word_count;
    }
    document_ids_.insert(document_id);
}

vector<Document> SearchServer::FindTopDocuments(string_view raw_query, DocumentStatus doc_status) const {
    return FindTopDocuments(raw_query,
        [doc_status](int document_id, DocumentStatus status, int rating)
        { return status == doc_status; }
    );
}
vector<Document> SearchServer::FindTopDocuments(const execution::parallel_policy policy, string_view raw_query, DocumentStatus doc_status) const {
    return FindTopDocuments(policy, raw_query,
        [doc_status](int document_id, DocumentStatus status, int rating)
        { return status == doc_status; }
    );
}
vector<Document> SearchServer::FindTopDocuments(const execution::sequenced_policy policy, string_view raw_query, DocumentStatus doc_status) const {
    return FindTopDocuments(policy, raw_query,
        [doc_status](int document_id, DocumentStatus status, int rating)
        { return status == doc_status; }
    );
}


std::vector<Document> SearchServer::FindTopDocuments(string_view raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}
std::vector<Document> SearchServer::FindTopDocuments(const execution::parallel_policy policy, string_view raw_query) const {
    return FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
}
std::vector<Document> SearchServer::FindTopDocuments(const execution::sequenced_policy policy, string_view raw_query) const {
    return FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
}


tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(string_view raw_query, int document_id) const {

    if (document_ids_.count(document_id) == 0) {
        throw invalid_argument("no document with such id");
    }
    Query query = ParseQuery(raw_query);
    const auto status = documents_.at(document_id).status;

    const auto WordChecker =
        [this, document_id](string_view word) {
        const auto it = word_to_documents_freqs_.find(word);
        return it != word_to_documents_freqs_.end() && it->second.count(document_id);
    };

    if (any_of(query.minus_words.begin(), query.minus_words.end(), WordChecker)) {
        return { {}, status };
    }


    vector<string_view> matched_words(query.plus_words.size());
    auto words_end = copy_if(
        query.plus_words.begin(), query.plus_words.end(),
        matched_words.begin(),
        WordChecker
    );
    sort(matched_words.begin(), words_end);
    words_end = unique(matched_words.begin(), words_end);
    matched_words.erase(words_end, matched_words.end());
    return { matched_words, status };

}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(const execution::parallel_policy&,
    string_view raw_query, int document_id) const
{

    if (document_ids_.count(document_id) == 0) {
        throw invalid_argument("no document with such id");
    }

    Query query = ParseQuery(raw_query);
    const auto status = documents_.at(document_id).status;

    const auto WordChecker =
        [this, document_id](string_view word) {
        const auto it = word_to_documents_freqs_.find(word);
        return it != word_to_documents_freqs_.end() && it->second.count(document_id);
    };

    if (any_of(execution::seq, query.minus_words.begin(), query.minus_words.end(), WordChecker)) {
        return { {}, status };
    }


    vector<string_view> matched_words(query.plus_words.size());
    auto words_end = copy_if(
        execution::par,
        query.plus_words.begin(), query.plus_words.end(),
        matched_words.begin(),
        WordChecker
    );
    sort(matched_words.begin(), words_end);
    words_end = unique(matched_words.begin(), words_end);
    matched_words.erase(words_end, matched_words.end());
    return { matched_words, status };

}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(const execution::sequenced_policy&,
    string_view raw_query, int document_id) const
{

    if (document_ids_.count(document_id) == 0) {
        throw invalid_argument("no document with such id");
    }

    Query query = ParseQuery(raw_query);
    const auto status = documents_.at(document_id).status;

    const auto WordChecker =
        [this, document_id](string_view word) {
        const auto it = word_to_documents_freqs_.find(word);
        return it != word_to_documents_freqs_.end() && it->second.count(document_id);
    };

    if (any_of(execution::seq, query.minus_words.begin(), query.minus_words.end(), WordChecker)) {
        return { {}, status };
    }


    vector<string_view> matched_words(query.plus_words.size());
    auto words_end = copy_if(
        execution::seq,
        query.plus_words.begin(), query.plus_words.end(),
        matched_words.begin(),
        WordChecker
    );
    sort(execution::par, matched_words.begin(), words_end);
    words_end = unique(matched_words.begin(), words_end);
    matched_words.erase(words_end, matched_words.end());

    return { matched_words, status };

}


int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

std::set<int>::const_iterator SearchServer::begin() const {
    return document_ids_.begin();
}

std::set<int>::const_iterator SearchServer::end() const {
    return document_ids_.end();
}


const std::map<std::string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
    static const map<string_view, double> empty_map = {};
    if (document_id_to_word_freqs_.find(document_id) == document_id_to_word_freqs_.end()) {
        return empty_map;
    }
    return document_id_to_word_freqs_.at(document_id);
}


void SearchServer::RemoveDocument(int document_id) {

    if (document_id_to_word_freqs_.count(document_id) == 0) {
        return;
    }

    /* { {word, TF}, {word, TF}, ... } */
    std::vector< std::pair<std::string, double> > words_and_it_freqs((document_id_to_word_freqs_.at(document_id)).size());
    copy((document_id_to_word_freqs_.at(document_id)).begin(),
        (document_id_to_word_freqs_.at(document_id)).end(),
        words_and_it_freqs.begin());

    std::for_each(
        words_and_it_freqs.begin(),
        words_and_it_freqs.end(),
        [&](const std::pair<std::string, double>& word_freq) {
            word_to_documents_freqs_[word_freq.first].erase(document_id);
        }
    );

    documents_.erase(document_id);
    document_ids_.erase(document_id);
    document_id_to_word_freqs_.erase(document_id);
}

void SearchServer::RemoveDocument(std::execution::parallel_policy policy, int document_id) {

    if (document_id_to_word_freqs_.count(document_id) == 0) {
        return;
    }

    /* { {word, TF}, {word, TF}, ... } */
    std::vector< std::pair<std::string, double> > words_and_it_freqs((document_id_to_word_freqs_.at(document_id)).size());
    copy(policy, (document_id_to_word_freqs_.at(document_id)).begin(),
        (document_id_to_word_freqs_.at(document_id)).end(),
        words_and_it_freqs.begin());

    std::for_each(
        policy,
        words_and_it_freqs.begin(),
        words_and_it_freqs.end(),
        [&](const std::pair<std::string, double>& word_freq) {
            word_to_documents_freqs_[word_freq.first].erase(document_id);
        }
    );

    documents_.erase(document_id);
    document_ids_.erase(document_id);
    document_id_to_word_freqs_.erase(document_id);
}


void SearchServer::RemoveDocument(std::execution::sequenced_policy policy, int document_id) {

    if (document_id_to_word_freqs_.count(document_id) == 0) {
        return;
    }

    /* { {word, TF}, {word, TF}, ... } */
    std::vector< std::pair<std::string, double> > words_and_it_freqs((document_id_to_word_freqs_.at(document_id)).size());
    copy(policy, (document_id_to_word_freqs_.at(document_id)).begin(),
        (document_id_to_word_freqs_.at(document_id)).end(),
        words_and_it_freqs.begin());

    std::for_each(
        policy,
        words_and_it_freqs.begin(),
        words_and_it_freqs.end(),
        [&](const std::pair<std::string, double>& word_freq) {
            word_to_documents_freqs_[word_freq.first].erase(document_id);
        }
    );

    documents_.erase(document_id);
    document_ids_.erase(document_id);
    document_id_to_word_freqs_.erase(document_id);
}



bool SearchServer::IsStopWord(const std::string& word) const {
    return stop_words_.count(word);
}



bool SearchServer::IsValidWord(const std::string& word) {
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
        });
}


vector<string_view> SearchServer::SplitIntoWordsNoStop(string_view text) const {
    vector<string_view> words;
    for (const string_view word : SplitIntoWordsView(text)) {
        if (!IsValidWord(string(word))) {
            throw invalid_argument("invalid characters");
        }
        if (!IsStopWord(string(word))) {
            words.push_back(word);
        }
    }
    return words;
}

SearchServer::Query SearchServer::ParseQuery(string_view query_string_view) const {
    Query query;

    if (!IsValidWord(string(query_string_view))) {
        throw invalid_argument("invalid characters");
    }

    vector<string_view> query_sv_cont = SplitIntoWordsView(query_string_view);

    for (string_view word : query_sv_cont) {
        if (word[0] == '-') {
            string_view minus_word = word.substr(1);
            if (minus_word.empty()) {
                throw invalid_argument("empty minus-word");
            }
            if (minus_word[0] == '-') {
                throw invalid_argument("two minuses in a row");
            }
            query.minus_words.push_back(minus_word);
            continue;
        }

        query.plus_words.push_back(word);
    }

    sort(query.plus_words.begin(), query.plus_words.end());
    auto plus_words_end = unique(query.plus_words.begin(), query.plus_words.end());
    query.plus_words.resize(distance(query.plus_words.begin(), plus_words_end));

    return query;
}


double SearchServer::CalcIDF(string_view word) const {
    return log((1.0 * SearchServer::GetDocumentCount()) / word_to_documents_freqs_.at(word).size());
}



int SearchServer::ComputeAverageRating(const vector<int>& marks) {
    int size = marks.size();
    int sum = accumulate(marks.begin(), marks.end(), 0);
    return sum / size;
}