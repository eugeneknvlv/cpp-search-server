#pragma once
#include <iostream>

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

struct DocumentProperties {
    int rating;
    DocumentStatus status;
};

std::ostream& operator<<(std::ostream& os, const Document& document);