#ifndef FILE_HANDLER_H
#define FILE_HANDLER_H

#include <iostream>
#include <fstream>
#include <utility>
using namespace std;

pair<char*,int> readFile(string fileName);
void writeFile(string fileName, char *payload, int fileSize);

#endif