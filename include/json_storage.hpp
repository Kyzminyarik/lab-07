#ifndef JSON_STORAGE_HPP_
#define JSON_STORAGE_HPP_
#include "nlohmann/json.hpp"
#include <iostream>
#include <string>

using json = nlohmann::json;

class JsonStorage{
public:
    JsonStorage(const std::string filename) : _filename(filename) {}
    json get_storage() const;
    void load();

    
private:
json _storage;
std::string _filename;
};


#endif // JSON_STORAGE_HPP_
