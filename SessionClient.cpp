#include <cctype>
#include <iostream>
#include <vector>
#include <exception>

#include "BlockStoreClient.cpp"

bool allDigit(std::string str) {
  for (int i = 0; i < str.size(); ++i) {
    if (!std::isdigit(str[i])) { return false; }
  }
  return true;
}

int main(int argc, char **argv) {
  BlockStoreClient hadev(0, false);
  while (true) {
    std::string request;
    size_t pos = 0;
    std::vector<std::string> words;
    std::cout << "next request: ";
    std::getline(std::cin, request);
    while ((pos = request.find(" ")) != std::string::npos) {
      words.push_back(request.substr(0, pos));
      request.erase(0, pos + 1);
    }
    words.push_back(request.substr(0, request.size()));
    if (!allDigit(words[1])) {
      std::cerr << "Error input\n";
    }
    else if (words.size() == 2 && words[0] == "r") {
      std::cout << hadev.Read(stoi(words[1])) << std::endl;
    }
    else if (words.size() == 3 && words[0] == "w") {
      std::cout << "Write "
                << (hadev.Write(stoi(words[1]), words[2]) ?
                   "failed" : "succeed")
                << std::endl;
    }
    else {
      std::cerr << "Error input\n";
    }
  }
}
