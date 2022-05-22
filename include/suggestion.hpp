// Copyright 2022 Kuzmin Yaroslav

#ifndef INCLUDE_SUGGESTION_HPP_
#define INCLUDE_SUGGESTION_HPP_

#include <iostream>
#include <string>
#include "nlohmann/json.hpp"

using json = nlohmann::json;

class suggestionsColl{
 public:
  suggestionsColl();
  void update(json store);
  json suggest(const std::string& input);

 private:
  json _suggestions;
};

auto example() -> void;

#endif // INCLUDE_SUGGESTION_HPP_
