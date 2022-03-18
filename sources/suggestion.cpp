// Copyright 2021 Your Name <your_email>

#include <stdexcept>

#include <suggestion.hpp>

suggestionsColl::suggestionsColl() { _suggestions = {}; }

void suggestionsColl::update(json storage) {
  std::sort(storage.begin(), storage.end(),
            [](const json& a, const json& b) -> bool {
              return a.at("cost") < b.at("cost");
            });
  _suggestions = storage;
}

json suggestionsColl::suggest(const std::string& input) {
  json res;
  for (size_t i = 0, m = 0; i < _suggestions.size(); ++i) {
    if (input == _suggestions[i].at("id")) {
      json sug;
      sug["text"] = _suggestions[i].at("name");
      sug["position"] = m++;
      res["suggestions"].push_back(sug);
    }
  }
  return res;
}

auto example() -> void {
  throw std::runtime_error("not implemented");
}
