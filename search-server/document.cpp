#include "document.h"

using namespace std;

Document::Document() 
    : id(0)
    , relevance(0)
    , rating(0)
{}

Document::Document(int id_init, double relevance_init, int rating_init)
    : id(id_init)
    , relevance(relevance_init)
    , rating(rating_init)
{}

ostream& operator<<(ostream& os, const Document& document) {
    os << "{ "s 
         << "document_id = "s << document.id << ", "s 
         << "relevance = "s << document.relevance << ", "s 
         << "rating = "s << document.rating 
         << " }"s; 

    return os;
}
