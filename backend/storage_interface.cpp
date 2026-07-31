#include <filesystem>
#include <fileapi.h>
#include <iostream>
#include <string>
#include <regex>

using namespace std;

#define FileService = {} \

namespace FileContext {
    static filesystem::path l_GetFilePath() {
        const regex FilePattern = regex("[^(/|\\)]+", "gm");
        cout << "PATH:\t" << FileContext::l_GetFilePath() << endl;
        return (filesystem::path(string("./")));
    }
}
