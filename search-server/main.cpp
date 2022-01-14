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
                if (word_to_documents_freqs_.count(word)) {
                    if (word_to_documents_freqs_.at(word).count(document_id)) {
                        result.push_back(word);
                    }
                }
            }

            for (const string& word : query.minus_words) {
                if (word_to_documents_freqs_.count(word)) {
                    if (word_to_documents_freqs_.at(word).count(document_id)) {
                        result.clear();
                        break;
                    }
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
                                            documents_.at(document_id).status
                                            }
                );
            }

            return matched_documents;
        }

};


#define RUN_TEST(func) RunTestImpl((func), #func)
template <typename TestFunc>
void RunTestImpl(TestFunc test_func, const string& func_name) {
    test_func();
    cerr << func_name << " OK"s << endl;
}


template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
                     const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cerr << boolalpha;
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cerr << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}
#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))


void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
                const string& hint) {
    if (!value) {
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}
#define ASSERT(expr) AssertImpl((expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_HINT(expr, hint) AssertImpl((expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))


// -------- Начало модульных тестов поисковой системы ----------

void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    // Сначала убеждаемся, что поиск слова, не входящего в список стоп-слов,
    // находит нужный документ
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    // Затем убеждаемся, что поиск этого же слова, входящего в список стоп-слов,
    // возвращает пустой результат
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("in"s).empty());
    }
}

void TestAddDocument() {
    SearchServer server;
    vector<int> ratings = {1, 2, 3};
    
    {
        server.AddDocument(42, "cute fluffy cat"s, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("cute"s);
        ASSERT_EQUAL(found_docs.size(), 1);
        ASSERT_EQUAL(found_docs[0].id, 42);
    }
    
    {
        server.AddDocument(43, "cat with big eyes"s, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("cat"s);
        ASSERT_EQUAL(found_docs.size(), 2);
    }

    {
        const auto found_docs = server.FindTopDocuments("cool dog"s);
        ASSERT(found_docs.empty());
    }
    
}

void TestExcludeDocumentsContainingMinusWords(){
    SearchServer server;
    vector<int> ratings = {1, 2, 3};

    server.AddDocument(42, "cute cat with collar", DocumentStatus::ACTUAL, ratings);
    server.AddDocument(24, "white fluffy cat", DocumentStatus::ACTUAL, ratings);
    const auto found_docs = server.FindTopDocuments("white cat -collar"s);
    ASSERT_EQUAL(found_docs.size(), 1);
    ASSERT_EQUAL(found_docs[0].id, 24);
}

void TestMatchDocuments() {
    SearchServer server;
    vector<int> ratings = {1, 2, 3};
    server.AddDocument(42, "cute cat with collar", DocumentStatus::ACTUAL, ratings);

    {
        const auto [matched_words, status] = server.MatchDocument("white cute cat"s, 42);
        ASSERT_EQUAL(count(matched_words.begin(), matched_words.end(), "cat"s), 1);
        ASSERT_EQUAL(count(matched_words.begin(), matched_words.end(), "cute"s), 1);
        ASSERT_EQUAL(count(matched_words.begin(), matched_words.end(), "with"s), 0);
        ASSERT_EQUAL(count(matched_words.begin(), matched_words.end(), "collar"s), 0);
    }

    {
        const auto [matched_words, status] = server.MatchDocument("white cute cat -collar"s, 42);
        ASSERT(matched_words.empty());
    }
}

void TestRelevanceCalculationAndSortFoundDocumentsByRelevance() {
    SearchServer server;
    vector<int> ratings = {1, 2, 3};

    server.AddDocument(1, "some yard cat"s, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(2, "cute fluffy cat"s, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(3, "cute black cat with green eyes"s, DocumentStatus::ACTUAL, ratings);

    const auto found_docs = server.FindTopDocuments("cute cat"s);
    double relevance1 = 0.0 / 3 * log( 3.0 / 2 ) + 1.0 / 3 * log( 3.0 / 3 ); //  1_st term: [doc 1 doesn't contain "cute" => TF(doc1, "cute") == 0
                                                                             //              two docs containe "cute" => IDF("cute") == log(3/2)];  
                                                                             //  2_nd term: [doc 1 includes "cat" once => TF(doc1, "cat") == 1/3. 
                                                                             //              all three docs contain "cat" => IDF("cat") == log(3/3) == 0];
                                                                             //  Overall: relevance = 1_st term + 2_nd term = 0 * log(3/2) + 1/3 * 0 = 0


    double relevance2 = (1.0 / 3) * log( 3.0 / 2 ) + (1.0 / 3) * log( 3.0 / 3 ); //  1_st term: [doc 2 includes "cute" once => TF(doc2, "cute") == 1/3
                                                                             //              two docs contain "cute" => IDF("cute") == log( 3/2 )];  
                                                                             //  2_nd term: [doc 2 includes "cat" once => TF(doc2, "cat") == 1/3. 
                                                                             //              all three docs contain "cat" => IDF("cat") == log( 3/3 ) == 0];
                                                                             //  Overall: relevance = 1_st term + 2_nd term = 1/3 * log(3/2) + 1/3 * 0 ~ 0.135155


    double relevance3 = (1.0 / 6) * log( 3.0 / 2 ) + (1.0 / 6) * log( 3.0 / 3); //  1_st term: [doc 3 includes "cute" once => TF(doc3, "cute") == 1/6
                                                                             //              two docs contain "cute" => IDF("cute") == log( 3/2 )];  
                                                                             //  2_nd term: [doc 3 includes "cat" once => TF(doc3, "cat") == 1/6. 
                                                                             //              all three docs contain "cat" => IDF("cat") == log( 3/3 ) == 0];
                                                                             //  Overall: relevance = 1_st term + 2_nd term = 1/6 * log(3/2) + 1/6 * 0 ~ 0.067576

    // relevance(doc1) = 0; relevance(doc2) ~ 0.135155; relevance(doc3) ~ 0.067576 => after sorting by relevance found_docs should look like [doc2, doc3, doc1]. Let's see.
    
    ASSERT(found_docs[0].id == 2);
    ASSERT(found_docs[1].id == 3);
    ASSERT(found_docs[2].id == 1);

    ASSERT_EQUAL(found_docs[0].relevance, relevance2);
    ASSERT_EQUAL(found_docs[1].relevance, relevance3);
    ASSERT_EQUAL(found_docs[2].relevance, relevance1);
}

void TestCalculateDocumentRating() {
    SearchServer server;
    vector<int> ratings = {3, 8, 9, 20};
    double average_rating = 1.0 * (3 + 8 + 9 + 20) / 4;
    
    server.AddDocument(1, "big brown dog"s, DocumentStatus::ACTUAL, ratings);
    const auto found_docs = server.FindTopDocuments("big brown dog"s);
    ASSERT_EQUAL(found_docs[0].rating, average_rating);
}

void TestFilterDocumentsByUsersPredicate() {
    SearchServer server;
    vector<int> ratings = {1, 2, 3};
    
    server.AddDocument(1, "some yard cat"s, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(2, "white fluffy cat"s, DocumentStatus::IRRELEVANT, ratings);
    server.AddDocument(3, "cute cat with big eyes"s, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(4, "cute cat with collar"s, DocumentStatus::ACTUAL, ratings);

    {
        const auto found_docs = server.FindTopDocuments("cat"s, [](int document_id, DocumentStatus status, 
                                                                    int rating) { return document_id % 2 == 0; }
                                                            );
        ASSERT_EQUAL(found_docs.size(), 2);
        ASSERT_EQUAL(found_docs[0].id, 2);
        ASSERT_EQUAL(found_docs[1].id, 4);
    }

    {
        const auto found_docs = server.FindTopDocuments("cat"s, [](int document_id, DocumentStatus status, 
                                                                    int rating) { return status == DocumentStatus::IRRELEVANT; }
                                                            );
        ASSERT_EQUAL(found_docs.size(), 1);
        ASSERT_EQUAL(found_docs[0].id, 2);
    }
}

void TestFindDocumentsWithSpecificStatus() {
    SearchServer server;
    vector<int> ratings = {1, 2, 3};
    
    server.AddDocument(1, "some yard cat"s, DocumentStatus::BANNED, ratings);
    server.AddDocument(2, "white fluffy cat"s, DocumentStatus::IRRELEVANT, ratings);
    server.AddDocument(3, "cute cat with big eyes"s, DocumentStatus::ACTUAL, ratings);

    {
        const auto found_docs = server.FindTopDocuments("cat"s, DocumentStatus::ACTUAL);
        ASSERT_EQUAL(found_docs.size(), 1);
        ASSERT_EQUAL(found_docs[0].id, 3);
    }

    {
        const auto found_docs = server.FindTopDocuments("cat"s, DocumentStatus::IRRELEVANT);
        ASSERT_EQUAL(found_docs.size(), 1);
        ASSERT_EQUAL(found_docs[0].id, 2);
    }

    {
        const auto found_docs = server.FindTopDocuments("cat"s, DocumentStatus::BANNED);
        ASSERT_EQUAL(found_docs.size(), 1);
        ASSERT_EQUAL(found_docs[0].id, 1);
    }
}


// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestAddDocument);
    RUN_TEST(TestExcludeDocumentsContainingMinusWords);
    RUN_TEST(TestMatchDocuments);
    RUN_TEST(TestRelevanceCalculationAndSortFoundDocumentsByRelevance);
    RUN_TEST(TestCalculateDocumentRating);
    RUN_TEST(TestFilterDocumentsByUsersPredicate);
    RUN_TEST(TestFindDocumentsWithSpecificStatus);
}

// --------- Окончание модульных тестов поисковой системы -----------

int main() {
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
}