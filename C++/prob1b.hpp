#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <vector>
#include <map>
#include <algorithm>

using namespace std;


char most_frequent_char(std::string input_sentence){
//gpt 풀이, sstream 없어도 쌉가능
    // char c;
    map<char,int> save;
    for(char x : input_sentence){
        if(x!=' '){
            save[x]++;
        }
    }

    size_t cnt=0;
    char ans;

    for(auto &pair : save){

        if(pair.second > cnt){
            cnt = pair.second;
            ans = pair.first;
        }
    }
    return ans;
/* 나의 풀이
    stringstream ss(input_sentence);
    map<char,int> save;
    char c;
    // while(ss >> c){
    //     if(c == ' ' || c == '\n') continue;        
    //     save[c]++; //map은 자동적으로 key 없을 때 key update 하면 초기값 0으로 초기화해줌
    // }
    while(ss >> c){
        if(c == ' ' || c == '\n') continue;        
        if(!save[c] && save[c]!=0){ //조건문 심도있게 넣기
            save[c]=0;
        }else save[c]++;
    }

    // for(auto &pair : save){
    //     cout << pair.first << " " << pair.second << endl;
    // }

    char ans;
    size_t count = 0;
    for(auto it = save.begin();it!=save.end();it++){
        if(it->second >count){
            ans = it->first;
            count = it->second;}
    }
    return ans;
    */
}