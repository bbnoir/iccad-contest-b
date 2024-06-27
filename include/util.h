#pragma once
#include <iostream>
#include <string>
// #include <boost/algorithm/string.hpp>
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
        const size_t len = str1.length();
        if (len != str2.length()) return false;
        for (size_t i = 0; i < len; ++i) {
            if (tolower(str1[i]) != tolower(str2[i])) return false;
        }
        return true;
    }
};