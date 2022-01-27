#include <algorithm>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <map>  
#include <cmath>
#include <numeric>
#include <cassert>
#include <tuple>
#include <locale>
#include <optional>


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

    Document() = default;

    Document(int id_init, double relevance_init, int rating_init) {
        id = id_init;
        relevance = relevance_init;
        rating = rating_init;
    }

    int id = 0;
    double relevance = 0;
    int rating = 0;
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

        explicit SearchServer(const string& stop_words_string) 
            : SearchServer(SplitIntoWords(stop_words_string))
        {}

        template <typename StringCollection>
        explicit SearchServer(const StringCollection& stop_words) {
            for (const string& word : stop_words)  {
                if (!IsValidWord(word)) {
                    throw invalid_argument("stop words contain invalid symbols");
                }
                stop_words_.insert(word);
            }
        }

        void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& marks) {
            if (document_id < 0 || documents_.count(document_id) > 0) {
                throw invalid_argument("invalid document id");
            }
            vector<string> words = SplitIntoWordsNoStop(document);       
            const double inv_word_count = 1.0 / words.size();           
            for (const string& word : words) {
                word_to_documents_freqs_[word][document_id] += inv_word_count;  // calculating Term Frequency         
            }
            int rating = ComputeAverageRating(marks);
            DocumentProperties properties = {rating, status};
            documents_.emplace(document_id, properties);
            document_ids_.push_back(document_id);
        }

        template <typename Predicate>
        vector<Document> FindTopDocuments(const string& raw_query, Predicate predicate) const {
            Query query = ParseQuery(raw_query);
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

        vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus doc_status) const {

             return FindTopDocuments(raw_query, 
                                     [doc_status](int document_id, DocumentStatus status, int rating)
                                     { return status == doc_status; } 
            );
        }

        vector<Document> FindTopDocuments(const string& raw_query) const {
            return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
        }

        tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
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
        
        int GetDocumentCount() const {
            return documents_.size();
        }

        int GetDocumentId(int index) const {
            if (index < 0 || index > GetDocumentCount()) {
                throw out_of_range("no document with such index");
            }
            return document_ids_[index];
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
        vector<int> document_ids_;

        bool IsStopWord(const string& word) const {
            return stop_words_.count(word);
        }

        static bool IsValidWord(const string& word) {
            return none_of(word.begin(), word.end(), [](char c) {
                return c >= '\0' && c < ' ';
            });
        }       

        vector<string> SplitIntoWordsNoStop(const string& text) const {
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

        Query ParseQuery(const string& query_string) const {
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

        double CalcIDF(const string& word) const {
            return log( (1.0 * GetDocumentCount()) / word_to_documents_freqs_.at(word).size() );
        }

        static int ComputeAverageRating(const vector<int>& marks) {
            int size = marks.size();
            int sum = accumulate(marks.begin(), marks.end(), 0);
            return sum / size;
        }

        template <typename Predicate>
        vector<Document> FindAllDocuments(const Query& query, Predicate predicate) const {

            map<int, double> doc_to_relevance; // { {document_id, relevance}, ... }

            for (const string& word : query.plus_words) {             
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
            for (const string& word : query.minus_words) { 
                for (const auto [document_id, TF] : word_to_documents_freqs_.at(word)) { // loop through all the documents, containing minus-words
                    doc_to_relevance.erase(document_id); // erase document, that contains minus-word, from chosen ones
                }
            }
            
            vector<Document> matched_documents;
            for (const auto [document_id, relevance] : doc_to_relevance) {
                matched_documents.push_back({document_id, relevance, 
                                            documents_.at(document_id).rating, 
                                            }
                );
            }

            return matched_documents;
        }

};


