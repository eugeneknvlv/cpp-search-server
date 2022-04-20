#pragma once
#include "document.h"
#include "string_processing.h"

#include <vector>
#include <set>
#include <unordered_set>
#include <map>
#include <deque>
#include <string>
#include <string_view>
#include <algorithm>
#include <stdexcept>
#include <execution>
#include <chrono>
#include <mutex>
#include "concurrent_map.h"

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPSILON = 1e-6;

class SearchServer {
public:

    explicit SearchServer(const std::string& stop_words_string);
    explicit SearchServer(std::string_view stop_words_string_view);

    template <typename StringCollection>
    explicit SearchServer(const StringCollection& stop_words);

    void AddDocument(int document_id, std::string_view document, DocumentStatus status, const std::vector<int>& marks);

    template <typename Predicate>
    std::vector<Document> FindTopDocuments(std::string_view raw_query, Predicate predicate) const;
    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentStatus doc_status) const;
    std::vector<Document> FindTopDocuments(std::string_view raw_query) const;
    template <typename Predicate>
    std::vector<Document> FindTopDocuments(std::execution::parallel_policy policy, std::string_view raw_query, Predicate predicate) const;
    std::vector<Document> FindTopDocuments(std::execution::parallel_policy policy, std::string_view raw_query, DocumentStatus doc_status) const;
    std::vector<Document> FindTopDocuments(std::execution::parallel_policy policy, std::string_view raw_query) const;
    template <typename Predicate>
    std::vector<Document> FindTopDocuments(std::execution::sequenced_policy policy, std::string_view raw_query, Predicate predicate) const;
    std::vector<Document> FindTopDocuments(std::execution::sequenced_policy policy, std::string_view raw_query, DocumentStatus doc_status) const;
    std::vector<Document> FindTopDocuments(std::execution::sequenced_policy policy, std::string_view raw_query) const;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::string_view raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::execution::parallel_policy& policy,
        std::string_view raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::execution::sequenced_policy& policy,
        std::string_view raw_query, int document_id) const;


    int GetDocumentCount() const;

    std::set<int>::const_iterator begin() const;
    std::set<int>::const_iterator end() const;

    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

    void RemoveDocument(int document_id);
    void RemoveDocument(std::execution::parallel_policy policy, int document_id);
    void RemoveDocument(std::execution::sequenced_policy policy, int document_id);

private:

    struct QueryWord {
        std::string_view text;
        bool is_minus;
        bool is_stop;
    };

    struct Query {
        std::deque<std::string_view> plus_words;
        std::deque<std::string_view> minus_words;
    };
    std::set<std::string, std::less<>> stop_words_;

    std::map<std::string_view, std::map<int, double>> word_to_documents_freqs_;
    std::map<int, DocumentData> documents_;
    std::set<int> document_ids_;
    std::map<int, std::map<std::string_view, double>> document_id_to_word_freqs_;

    bool IsStopWord(const std::string& word) const;

    static bool IsValidWord(const std::string& word);

    std::vector<std::string_view> SplitIntoWordsNoStop(std::string_view text) const;

    Query ParseQuery(std::string_view query_string) const;

    double CalcIDF(std::string_view word) const;

    static int ComputeAverageRating(const std::vector<int>& marks);

    template <typename Predicate>
    std::vector<Document> FindAllDocuments(const Query& query, Predicate predicate) const;
    template <typename Predicate>
    std::vector<Document> FindAllDocuments(std::execution::parallel_policy& policy, const Query& query, Predicate predicate) const;

};


template <typename StringCollection>
SearchServer::SearchServer(const StringCollection& stop_words)
    : stop_words_(MakeUniqueNonEmptyStrings(stop_words))
{
    if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
        using namespace std;
        throw std::invalid_argument("invalid symbols in stop words");
    }
}

template <typename Predicate>
std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, Predicate predicate) const {
    Query query = ParseQuery(raw_query);
    std::vector<Document> matched_documents = FindAllDocuments(query, predicate);

    sort(matched_documents.begin(),
        matched_documents.end(),
        [](const Document& lhs, const Document& rhs) {
            double err = std::abs(lhs.relevance - rhs.relevance);
            if (err < EPSILON) {
                return lhs.rating > rhs.rating;
            }
            return (lhs.relevance > rhs.relevance);
        }
    );

    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }

    return matched_documents;
}

template <typename Predicate>
std::vector<Document> SearchServer::FindTopDocuments(std::execution::sequenced_policy policy, std::string_view raw_query, Predicate predicate) const {
    return FindTopDocuments(raw_query, predicate);
}

template <typename Predicate>
std::vector<Document> SearchServer::FindTopDocuments(std::execution::parallel_policy policy,
    std::string_view raw_query,
    Predicate predicate) const
{
    Query query = ParseQuery(raw_query);
    std::vector<Document> matched_documents = FindAllDocuments(policy, query, predicate);

    sort(policy,
        matched_documents.begin(),
        matched_documents.end(),
        [](const Document& lhs, const Document& rhs) {
            double err = std::abs(lhs.relevance - rhs.relevance);
            if (err < EPSILON) {
                return lhs.rating > rhs.rating;
            }
            return (lhs.relevance > rhs.relevance);
        }
    );

    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }

    return matched_documents;
}


template <typename Predicate>
std::vector<Document> SearchServer::FindAllDocuments(const Query& query, Predicate predicate) const {

    std::map<int, double> doc_to_relevance;

    for (std::string_view word : query.plus_words) {
        if (word_to_documents_freqs_.count(word) == 0) {
            continue;
        }

        double IDF = CalcIDF(word);
        for (const auto& [document_id, TF] : word_to_documents_freqs_.at(word)) {
            const auto& document = documents_.at(document_id);
            if (predicate(document_id, document.status, document.rating)) {
                doc_to_relevance[document_id] += TF * IDF;
            }
        }
    }
    for (std::string_view word : query.minus_words) {
        for (const auto& [document_id, TF] : word_to_documents_freqs_.at(word)) {
            doc_to_relevance.erase(document_id);
        }
    }

    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : doc_to_relevance) {
        matched_documents.push_back({ document_id, relevance,
                                    documents_.at(document_id).rating,
            }
        );
    }

    return matched_documents;
}

template <typename Predicate>
std::vector<Document> SearchServer::FindAllDocuments(std::execution::parallel_policy& policy, const Query& query, Predicate predicate) const {

    ConcurrentMap<int, double> doc_to_relevance(10);

    std::for_each(
        policy,
        query.plus_words.begin(), query.plus_words.end(),
        [this, predicate, &doc_to_relevance](std::string_view word) {
            if (word_to_documents_freqs_.count(word)) {
                double IDF = CalcIDF(word);
                for (const auto& [document_id, TF] : word_to_documents_freqs_.at(word)) {
                    const auto& document = documents_.at(document_id);
                    if (predicate(document_id, document.status, document.rating)) {
                        doc_to_relevance[document_id].ref_to_value += TF * IDF;
                    }
                }
            }
        }
    );

    std::map<int, double> doc_to_relevance_ord_map = doc_to_relevance.BuildOrdinaryMap();

    for (std::string_view word : query.minus_words) {
        for (const auto& [document_id, TF] : word_to_documents_freqs_.at(word)) {
            doc_to_relevance_ord_map.erase(document_id);
        }
    }

    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : doc_to_relevance_ord_map) {
        matched_documents.push_back({ document_id, relevance,
                                    documents_.at(document_id).rating,
            }
        );
    }

    return matched_documents;
}

