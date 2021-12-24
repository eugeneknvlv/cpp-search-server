#include <algorithm>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <map>  
#include <cmath>
#include <numeric>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPSILON = 1e-6;

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED
};

struct Document {
    int id;
    double relevance;
    int rating;
    DocumentStatus status;
};

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result = 0;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

class SearchServer {
public:

        void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& marks) {
            const vector<string> words = SplitIntoWordsNoStop(document);
            const double inv_word_count = 1.0 / words.size();           
            for (const string& word : words) {
                word_to_documents_freqs_[word][document_id] += inv_word_count;  // calculating Term Frequency         
            }
            int rating = ComputeAverageRating(marks);
            DocumentProperties properties = {rating, status};
            documents_.emplace(document_id, properties);
        }

        void SetStopWords(const string& stop_words) {
            for (const string& word : SplitIntoWords(stop_words)) {
                stop_words_.insert(word);
            }
        }


        template <typename Predicate>
        vector<Document> FindTopDocuments(const string& query, Predicate predicate) const {
            vector<Document> matched_documents = FindAllDocuments(query, predicate);

            sort(matched_documents.begin(),
                 matched_documents.end(),
                 [](const Document& lhs, const Document& rhs) {
                    double err = abs(lhs.relevance - rhs.relevance);
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

        vector<Document> FindTopDocuments(const string& query, DocumentStatus doc_status = DocumentStatus::ACTUAL) const {
            return FindTopDocuments(query, [doc_status](int document_id, DocumentStatus status, int rating) { return status == doc_status; });
        }

        tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
            vector<string> result;
            const Query query = ParseQuery(raw_query);

            for (const string& word : query.plus_words) {
                if (word_to_documents_freqs_.at(word).count(document_id)) {
                    result.push_back(word);
                }
            }

            for (const string& word : query.minus_words) {
                if (word_to_documents_freqs_.at(word).count(document_id)) {
                    result.clear();
                    break;
                }
            }
            
            sort(result.begin(), result.end());
            return tuple<vector<string>, DocumentStatus>(result, documents_.at(document_id).status);
        }
        
        int GetDocumentCount() const {
            return documents_.size();
        }

private:
        struct DocumentProperties {
            int rating;
            DocumentStatus status;
        };

        struct Query {
            set<string> plus_words;
            set<string> minus_words;
        };
        
        map<string, map<int, double>> word_to_documents_freqs_; // { {word, {document_id, TF}}, ... }
        set<string> stop_words_;
        map<int, DocumentProperties> documents_; // { {document_id, {rating, status}}, ... }

        bool IsStopWord(const string& word) const {
            return stop_words_.count(word);
        }

        vector<string> SplitIntoWordsNoStop(const string& text) const {
            vector<string> words;
            for (const string& word : SplitIntoWords(text)) {
                if (!IsStopWord(word)) {
                    words.push_back(word);
                } 
            }
            return words;
        }

        Query ParseQuery(const string& query_string) const {
            Query query;
            for (const string& word : SplitIntoWords(query_string)) {
                if (word[0] == '-') {
                    query.minus_words.insert(word.substr(1));
                    continue;
                }
                query.plus_words.insert(word);
            }
            return query;
        }

        double CalcIDF(const string& word) const {
            return log( (1.0 * GetDocumentCount()) / word_to_documents_freqs_.at(word).size() );
        }

        static int ComputeAverageRating(const vector<int>& marks) {
            int size = marks.size();
            int sum = accumulate(marks.begin(), marks.end(), 0);
            return sum / size;
        }

        template <typename Predicate>
        vector<Document> FindAllDocuments(const string& query_string, Predicate predicate) const {

            map<int, double> doc_to_relevance; // { {document_id, relevance}, ... }

            Query query = ParseQuery(query_string); // result is the struct {plus_words, minus_words}
            for (const string& word : query.plus_words) {             
                if (word_to_documents_freqs_.count(word) == 0) { // if there's no sought word in any document, continue
                    continue;
                }

                double IDF = CalcIDF(word);
                for (const auto& [document_id, TF] : word_to_documents_freqs_.at(word)) {
                    if (predicate(document_id, documents_.at(document_id).status, documents_.at(document_id).rating)) {
                        doc_to_relevance[document_id] += TF * IDF;
                    }
                }
            }
            for (const string& word : query.minus_words) { 
                for (const auto [document_id, TF] : word_to_documents_freqs_.at(word)) { // loop through all the documents, containing minus-words
                    doc_to_relevance.erase(document_id); // erase document, that contains minus-word, from chosen ones
                }
            }
            
            vector<Document> matched_documents;
            for (auto [document_id, relevance] : doc_to_relevance) {
                matched_documents.push_back({document_id, relevance, 
                                            documents_.at(document_id).rating, 
                                            documents_.at(document_id).status
                                            }
                );
            }

            return matched_documents;
        }

};


void PrintDocument(const Document& document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating
         << " }"s << endl;
}

int main() {
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);

    search_server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    search_server.AddDocument(3, "ухоженный скворец евгений"s,         DocumentStatus::BANNED, {9});

    cout << "ACTUAL by default:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s)) {
        PrintDocument(document);
    }

    cout << "BANNED:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::BANNED)) {
        PrintDocument(document);
    }

    cout << "Even ids:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; })) {
        PrintDocument(document);
    }

    return 0;
}