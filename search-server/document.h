#pragma once
#include <iostream>
#include <string>

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED
};

struct Document {
    Document();
    Document(int id_init, double relevance_init, int rating_init);

    int id;
    double relevance;
    int rating;
};

struct DocumentData {
    int rating;
    DocumentStatus status;
    std::string doc_text;
};

std::ostream& operator<<(std::ostream& os, const Document& document);