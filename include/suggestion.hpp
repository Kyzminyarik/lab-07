// Copyright 2021 Your Name <your_email>

#ifndef INCLUDE_EXAMPLE_HPP_
#define INCLUDE_EXAMPLE_HPP_

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

#endif // INCLUDE_EXAMPLE_HPP_
