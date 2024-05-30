#pragma once
#include <iostream>
#include <string>
#include <boost/algorithm/string.hpp>
using std::string;
/************************************************************************/
/* Comparator for case-insensitive comparison in STL assos. containers  */
/************************************************************************/
struct ciLessLibC : public std::binary_function<string, string, bool> {
    bool operator()(const string &lhs, const string &rhs) const {
        return strcasecmp(lhs.c_str(), rhs.c_str()) < 0 ;
    }
};
