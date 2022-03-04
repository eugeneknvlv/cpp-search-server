#pragma once
#include <iostream>
#include <vector>
#include <cassert>

template <typename Iterator>
class IteratorRange {
public:
    IteratorRange(Iterator page_begin, Iterator page_end) 
        : page_( {page_begin, page_end} )
    {
    }

    Iterator begin() const {
        return page_.first;
    }

    Iterator end() const {
        return page_.second;
    }

private:
    std::pair<Iterator, Iterator> page_;
};

template <typename Iterator>
class Paginator {
public:
    Paginator(Iterator range_begin, Iterator range_end, int page_size) 
        : range_begin_(range_begin)
        , range_end_(range_end)
        , page_size_(page_size)
    {   
        assert(range_end >= range_end && page_size > 0);
        Iterator begin_it = range_begin;
        Iterator end_it = range_end;
        while (distance(begin_it, end_it) > page_size) {
            Iterator page_begin = begin_it;
            Iterator page_end = page_begin;
            advance(page_end, page_size);
            IteratorRange<Iterator> page(page_begin, page_end);
            pages_.push_back(page);
            advance(begin_it, page_size);
        }
        if (distance(begin_it, end_it) > 0) {
            IteratorRange<Iterator> page(begin_it, end_it);
            pages_.push_back(page);
        }
    }

    auto begin() const {
        return std::begin(pages_);
    }

    auto end() const {
        return std::end(pages_);
    }


private:
    Iterator range_begin_, range_end_;
    std::vector<IteratorRange<Iterator>> pages_;
    int page_size_;
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}

template <typename Iterator>
std::ostream& operator<<(std::ostream& os, const IteratorRange<Iterator>& iterator_range) {
    for (Iterator it = iterator_range.begin(); it < iterator_range.end(); it++) {
        os << *it;
    }
    return os;
}

