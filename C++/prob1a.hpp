#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <vector>
#include <map>
#include <algorithm>

using namespace std;

std::string longest_word(std::string input_sentence){
//GPT 답안

    stringstream ss(input_sentence);
    string word;
    string longest;
    size_t intmax = 0;

    while(ss >> word){
        if(word.length() > intmax){
            longest = word;
            intmax = word.length();
        }
    }
    return longest;

/*  //아래는 나의 답안, 정답은 나옴
    stringstream ss(input_sentence);

    vector<string> save;
    string word;

    while(ss){
        ss >> word;
        save.push_back(word);
    }
    
    int length = 0;
    string ans;
    for(auto x : save){
        if(x.length()>=length){
            length = x.length();
            ans = x;
        }
    }
    return ans;
*/
}