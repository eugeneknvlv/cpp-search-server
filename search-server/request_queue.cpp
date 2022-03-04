#include "request_queue.h"

using namespace std;

RequestQueue::RequestQueue(const SearchServer& search_server) 
    : search_server_(search_server)
    , current_time_(0)
    , no_result_requests_count_(0)
{
}

int RequestQueue::GetNoResultRequests() const {
    return no_result_requests_count_;
}

vector<Document> RequestQueue::AddFindRequest(const string& raw_query, DocumentStatus status) {
    return RequestQueue::AddFindRequest(raw_query, 
                            [status](int document_id, DocumentStatus doc_status, int rating)
                            { return doc_status == status; } 
    ); 
}

vector<Document> RequestQueue::AddFindRequest(const string& raw_query) {
    return RequestQueue::AddFindRequest(raw_query, DocumentStatus::ACTUAL);  
}