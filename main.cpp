#include <stdio.h>
#include <vector>
#include <string>
#include <unordered_map>

#define COUNTOF(X) (sizeof(X) / sizeof(X[0]))

typedef std::unordered_map<std::string, size_t> TWordCounts;

std::unordered_map<std::string, TWordCounts> g_wordCounts;

bool IsGoodChar(char c)
{
    if (c >= 'a' && c <= 'z')
        return true;

    if (c >= 'A' && c <= 'Z')
        return true;

    if (c >= '0' && c <= '9')
        return true;

    return false;
}

void GetWord(unsigned char* contents, size_t size, size_t& position, std::string& word)
{
    // skip bad characters to start
    while (position < size && !IsGoodChar(contents[position]))
        position++;
    size_t startPosition = position;

    // go until bad character, or end of data
    while (position < size && IsGoodChar(contents[position]))
        position++;

    // copy the word into the string
    word = std::string(&contents[startPosition], &contents[position]);
}

bool ProcessFile(const char* fileName)
{
    // read the file into memory
    FILE* file = nullptr;
    fopen_s(&file, fileName, "rt");
    if (!file)
        return false;
    std::vector<unsigned char> contents;
    fseek(file, 0, SEEK_END);
    contents.resize(ftell(file));
    fseek(file, 0, SEEK_SET);
    fread(contents.data(), 1, contents.size(), file);
    fclose(file);

    // process the file
    size_t position = 0;
    size_t size = contents.size();
    std::string nextWord, lastWord;
    GetWord(contents.data(), size, position, lastWord);
    while (position < size)
    {
        GetWord(contents.data(), size, position, nextWord);
        g_wordCounts[lastWord][nextWord]++;
        lastWord = nextWord;
    }

    return true;
}

bool GenerateFile(const char* outFileName, size_t wordLength)
{

}

int main(int argc, char** argv)
{
    const char* inputFiles[] =
    {
        "data/projbluenoise.txt"
    };

    for (size_t i = 0; i < COUNTOF(inputFiles); ++i)
    {
        printf("processing %s...\n", inputFiles[i]);
        if (!ProcessFile(inputFiles[i]))
        {
            printf("could not open file %s!\n", inputFiles[i]);
            return 1;
        }
    }

    if (!GenerateFile("out.txt", 1000))
    {
        printf("Could not generate output file!\n");
        return 1;
    }

    return 0;
}

/*

TODO:

* Nth order!
* more source texts (chanel's psych report. some poems?)
* make it generate output

*/