#include "process_queries.h"
#include <execution>
#include <list>
#include <utility>

using namespace std;

std::vector<std::vector<Document>> ProcessQueries(
    const SearchServer& search_server,
    const std::vector<std::string>& queries)
{
    vector<vector<Document>> query_results(queries.size());
    transform(execution::par, queries.begin(), queries.end(), query_results.begin(),
                [&search_server](const string& query) {
                    return search_server.FindTopDocuments(query);
                }   
    );
    return query_results;
}

vector<Document> ProcessQueriesJoined(
    const SearchServer& search_server,
    const std::vector<std::string>& queries)
{   
    vector<vector<Document>> processed_queries = ProcessQueries(search_server, queries);
    vector<Document> flat_queries_results;
    for (const auto& documents : processed_queries) {
        flat_queries_results.insert(flat_queries_results.end(), 
                                    make_move_iterator(documents.begin()), make_move_iterator(documents.end()));
    }
    return flat_queries_results;
}