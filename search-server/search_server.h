#pragma once
#include "document.h"
#include <vector>
#include <set>
#include <map>
#include <algorithm>
#include <stdexcept>

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPSILON = 1e-6;

class SearchServer {
public:

        explicit SearchServer(const std::string& stop_words_string);

        template <typename StringCollection>
        explicit SearchServer(const StringCollection& stop_words);

        void AddDocument(int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& marks);

        template <typename Predicate>
        std::vector<Document> FindTopDocuments(const std::string& raw_query, Predicate predicate) const;

        std::vector<Document> FindTopDocuments(const std::string& raw_query, DocumentStatus doc_status) const;

        std::vector<Document> FindTopDocuments(const std::string& raw_query) const;

        std::tuple<std::vector<std::string>, DocumentStatus> MatchDocument(const std::string& raw_query, int document_id) const;   

        int GetDocumentCount() const;

        std::vector<int>::const_iterator begin() const;

        std::vector<int>::const_iterator end() const;

        const std::map<std::string, double>& GetWordFrequencies(int document_id) const;

        void RemoveDocument(int document_id);

        std::vector<int> FindDuplicates();

private:

        struct Query {
            std::set<std::string> plus_words;
            std::set<std::string> minus_words;
        };
        
        std::map<std::string, std::map<int, double>> word_to_documents_freqs_; // { {word, {document_id, TF(word)}}, ... }
        std::set<std::string> stop_words_;
        std::map<int, DocumentProperties> documents_; // { {document_id, {rating, status}}, ... }
        std::vector<int> document_ids_;
        std::map<int, std::map<std::string, double>> document_id_to_word_freqs_; // { {document_id, {word, TF(word)}}, ... }
        std::map<int, std::set<std::string>> documents_content_;



        bool IsStopWord(const std::string& word) const;

        static bool IsValidWord(const std::string& word);

        std::vector<std::string> SplitIntoWordsNoStop(const std::string& text) const;

        Query ParseQuery(const std::string& query_string) const;

        double CalcIDF(const std::string& word) const;

        static int ComputeAverageRating(const std::vector<int>& marks);

        template <typename Predicate>
        std::vector<Document> FindAllDocuments(const Query& query, Predicate predicate) const;

};



template <typename StringCollection>
SearchServer::SearchServer(const StringCollection& stop_words) {
    for (const std::string& word : stop_words)  {
        if (!IsValidWord(word)) {
            throw std::invalid_argument("stop words contain invalid symbols");
        }
        stop_words_.insert(word);
    }
}



template <typename Predicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query, Predicate predicate) const {
    Query query = ParseQuery(raw_query);
    std::vector<Document> matched_documents = FindAllDocuments(query, predicate);

    sort(matched_documents.begin(),
            matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
            double err = std::abs(lhs.relevance - rhs.relevance);
            if (err < EPSILON) {
                return lhs.rating > rhs.rating; // if the difference of relevances is within the error, sort by ratings
            }
            return (lhs.relevance > rhs.relevance); // else sort by relevance
            }
    );

    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }

    return matched_documents;
}



template <typename Predicate>
std::vector<Document> SearchServer::FindAllDocuments(const Query& query, Predicate predicate) const {

    std::map<int, double> doc_to_relevance; // { {document_id, relevance}, ... }

    for (const std::string& word : query.plus_words) {             
        if (word_to_documents_freqs_.count(word) == 0) { // if there's no sought word in any document, continue
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
    for (const std::string& word : query.minus_words) { 
        for (const auto [document_id, TF] : word_to_documents_freqs_.at(word)) { // loop through all the documents, containing minus-words
            doc_to_relevance.erase(document_id); // erase document, that contains minus-word, from chosen ones
        }
    }
    
    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : doc_to_relevance) {
        matched_documents.push_back({document_id, relevance, 
                                    documents_.at(document_id).rating, 
                                    }
        );
    }

    return matched_documents;
}