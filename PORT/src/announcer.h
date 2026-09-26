#pragma once
#include <deque>
#include <string>
#include <unordered_map>

struct Mix_Chunk;

// Optional original recordings. Missing media leaves normal imports playable.
class Announcer {
public:
    explicit Announcer(const std::string& assets_dir);
    ~Announcer();
    void select(char slot);
    void fight();
    void victory(char slot);
    void update();
    void clear();
    bool available() const { return !names_.empty() || !victories_.empty(); }
private:
    std::string root_;
    std::unordered_map<char,std::string> names_, victories_;
    std::unordered_map<std::string,std::string> events_;
    std::unordered_map<std::string,Mix_Chunk*> chunks_;
    std::deque<std::string> queue_;
    void enqueue(const std::string& file);
};
