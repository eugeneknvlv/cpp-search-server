#pragma once
#include <deque>
#include <vector>
#include <string>
#include "search_server.h"

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server);
    
    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate);

    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);

    std::vector<Document> AddFindRequest(const std::string& raw_query);

    int GetNoResultRequests() const;

private:
    struct QueryResult {
        uint64_t timestamp;
        size_t results; 
    };
    std::deque<QueryResult> requests_;
    const static int min_in_day_ = 1440;
    const SearchServer& search_server_;
    uint64_t current_time_;
    int no_result_requests_count_; 
};



template <typename DocumentPredicate>
std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
    const std::vector<Document> found_documents = search_server_.FindTopDocuments(raw_query, document_predicate);
    ++current_time_;
    if (current_time_ > min_in_day_) {
        if (requests_.front().results == 0) {
            --no_result_requests_count_;
        }
        requests_.pop_front();
    }

    if (found_documents.empty()) {
        requests_.push_back({current_time_, found_documents.size()});
        ++no_result_requests_count_;
    }
    else {
        requests_.push_back({current_time_, found_documents.size()});
    }  
    return found_documents;
}


