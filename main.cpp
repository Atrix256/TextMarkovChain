#include <stdio.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <random>
#include <algorithm>

#define COUNTOF(X) (sizeof(X) / sizeof(X[0]))

typedef std::unordered_map<std::string, size_t> TWordCounts;

struct WordProbability
{
    std::string word;
    float probability;
};

typedef std::vector<WordProbability> TWordProbabilities;

std::unordered_map<std::string, TWordCounts> g_wordCounts;
std::unordered_map<std::string, TWordProbabilities> g_wordProbabilities;

bool IsGoodChar(char c)
{
    if (c >= 'a' && c <= 'z')
        return true;

    if (c >= 'A' && c <= 'Z')
        return true;

    if (c >= '0' && c <= '9')
        return true;

    if (c == '\'')
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

void MakeProbabilities()
{
    for (auto firstWord : g_wordCounts)
    {
        size_t sum = 0;
        for (auto nextWord : firstWord.second)
            sum += nextWord.second;

        float probabilitySum = 0.0f;
        for (auto nextWord : firstWord.second)
        {
            float probability = float(nextWord.second) / float(sum);
            probabilitySum += probability;
            g_wordProbabilities[firstWord.first].push_back(WordProbability{ nextWord.first, probabilitySum });
        }
    }
}

bool GenerateFile(const char* fileName, size_t wordCount)
{
    static std::random_device rd("dev/random");
    static std::seed_seq fullSeed{ rd(), rd(), rd(), rd(), rd(), rd(), rd(), rd() };
    static std::mt19937 rng(fullSeed);

    FILE* file = nullptr;
    fopen_s(&file, fileName, "w+t");
    if (!file)
        return false;

    std::uniform_int_distribution<size_t> dist(0, g_wordProbabilities.size());
    std::uniform_real_distribution<float> distFloat(0.0f, 1.0f);

    size_t lastWordIndex = dist(rng);
    auto it = g_wordProbabilities.begin();
    std::advance(it, lastWordIndex);
    fprintf(file, "%s ", it->first.c_str());
    std::string lastWord = it->first;

    for (size_t wordIndex = 0; wordIndex < wordCount; ++wordIndex)
    {
        TWordProbabilities probabilities = g_wordProbabilities[lastWord];
        if (probabilities.size() == 0)  // edge case: the only time a word appears is at the end of the file!
            break;

        float nextWordProbability = distFloat(rng);

        int nextWordIndex = 0;
        while (nextWordIndex < probabilities.size() - 1 && probabilities[nextWordIndex].probability < nextWordProbability)
            ++nextWordIndex;

        fprintf(file, "%s ", probabilities[nextWordIndex].word.c_str());
        lastWord = probabilities[nextWordIndex].word;
    }

    fclose(file);
    return true;
}

int main(int argc, char** argv)
{
    const char* inputFiles[] =
    {
        "data/projbluenoise.txt"
        //"data/psychreport.txt"
    };

    // process input
    for (size_t i = 0; i < COUNTOF(inputFiles); ++i)
    {
        printf("processing %s...\n", inputFiles[i]);
        if (!ProcessFile(inputFiles[i]))
        {
            printf("could not open file %s!\n", inputFiles[i]);
            return 1;
        }
    }

    // make probabilities
    printf("Calculating probabilities...\n");
    MakeProbabilities();

    // make output
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
* what about periods and commas and things
* maybe also handle capitalization.


* could also try doing something like... "here are the last 5 characters. here are characters at -10, -15, -20, -25 strings"

*/