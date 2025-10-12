#include "header/file-handler.h"
#include <cstring>

pair<char*,int> readFile(string fileName) {
    ifstream file(fileName);
    if(!file.is_open()){
        return pair<char*,int>(0,0);
    }
    file.seekg(0,ios::end);
    streamsize fsize = file.tellg();
    file.seekg(0,ios::beg);

    char *s = new char[fsize];
    file.read(s,fsize);
    
    return pair<char*,int>(s,fsize);
}

void writeFile(string FileName,char *payload,int fileSize){
    ofstream file(FileName);
    if(!file.is_open())return;
    file.write(payload,fileSize);
}