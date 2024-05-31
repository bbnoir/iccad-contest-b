#pragma once
#include <iostream>
#include <string>
#include <boost/algorithm/string.hpp>
using std::string;
/************************************************************************/
/* Comparator for case-insensitive comparison in STL assos. containers  */
/************************************************************************/
// Custom case-insensitive hash function
struct CIHash {
    std::size_t operator()(const std::string& str) const {
        std::size_t hash = 0;
        std::hash<char> char_hash;
        for (char c : str) {
            hash ^= char_hash(tolower(c)) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        }
        return hash;
    }
};

// Custom case-insensitive equality function
struct CIEqual {
    bool operator()(const std::string& str1, const std::string& str2) const {
        return std::equal(str1.begin(), str1.end(), str2.begin(), str2.end(),
            [](char c1, char c2) {
                return tolower(c1) == tolower(c2);
            });
    }
};