#include <stdio.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <random>
#include <algorithm>
#include <array>

template <typename TObservedState, typename TNextState>
class MarkovChain
{
public:

    MarkovChain()
        : m_rd("dev/random")
        , m_fullSeed{ m_rd(), m_rd(), m_rd(), m_rd(), m_rd(), m_rd(), m_rd(), m_rd() }
        , m_rng(m_fullSeed)
    {

    }

    // learning stage
    void Observe(TObservedState& observed, const TNextState& next)
    {
        if (ShouldObserve(observed))
            m_counts[observed][next]++;
        PostObserve(observed, next);
    }

    void FinalizeLearning()
    {
        // turn the sums into cumulative probabilities
        for (auto observed : m_counts)
        {
            size_t sum = 0;
            for (auto nextState : observed.second)
                sum += nextState.second;

            float probabilitySum = 0.0f;
            for (auto nextState : observed.second)
            {
                float probability = float(nextState.second) / float(sum);
                probabilitySum += probability;
                m_probabilities[observed.first].push_back(ObservedProbability{ nextState.first, probabilitySum });
            }
        }
    }

    // data generation stage
    TObservedState GetInitialState()
    {
        // select a starting state entirely at random
        std::uniform_int_distribution<size_t> dist(0, m_probabilities.size());
        size_t index = dist(m_rng);
        auto it = m_probabilities.begin();
        std::advance(it, index);
        return it->first;
    }

    TNextState GetNextState(const TObservedState& observed)
    {
        // get the next state by choosing a weighted random next state.
        std::uniform_real_distribution<float> distFloat(0.0f, 1.0f);
        TObservedProbabilities probabilities = m_probabilities[observed];

        float nextStateProbability = distFloat(m_rng);
        int nextStateIndex = 0;
        while (nextStateIndex < probabilities.size() - 1 && probabilities[nextStateIndex].cumulativeProbability < nextStateProbability)
            ++nextStateIndex;

        return probabilities[nextStateIndex].observed;
    }

    // virtual interface
    virtual bool ShouldObserve(const TObservedState& observed) { return true; }
    virtual void PostObserve(TObservedState& observed, const TNextState& next) = 0;

    // random number generation storage
    std::random_device m_rd;
    std::seed_seq m_fullSeed;
    std::mt19937 m_rng;

    // data storage
    typedef std::unordered_map<TObservedState, size_t> TObservedCounts;

    struct ObservedProbability
    {
        TObservedState observed;
        float cumulativeProbability;
    };

    typedef std::vector<ObservedProbability> TObservedProbabilities;

    std::unordered_map<TObservedState, TObservedCounts> m_counts;
    std::unordered_map<TObservedState, TObservedProbabilities> m_probabilities;
};

class TextMarkovChain : public MarkovChain<std::string, std::string>
{
public:

    virtual void PostObserve(std::string& observed, const std::string& next)
    {
        observed = next;
    }
};

class TextMarkovChain2dOrder : public MarkovChain<std::array<std::string, 2>, std::string>
{
public:

    virtual bool ShouldObserve(const std::array<std::string, 2>& observed)
    {
        return !observed[0].empty() && !observed[1].empty();
    }

    virtual void PostObserve(std::array<std::string, 2>& observed, const std::string& next)
    {
        observed[1] = observed[0];
        observed[0] = next;
    }
};

TextMarkovChain g_markovChain;
//TextMarkovChain2dOrder g_markovChain2ndOrder;

bool IsAlphaNumeric(char c)
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

bool IsPunctuation(char c)
{
    if (c == '.')
        return true;

    if (c == ',')
        return true;

    if (c == ';')
        return true;

    //if (c == '\"')
    //    return true;

    if (c == ':')
        return true;

    if (c == '-')
        return true;

    return false;
}

void GetWord(unsigned char* contents, size_t size, size_t& position, std::string& word)
{
    // skip ignored characters to start
    while (position < size && !IsAlphaNumeric(contents[position]) && !IsPunctuation(contents[position]))
        position++;

    // exit if there is no word
    if (position >= size)
    {
        word = "";
        return;
    }

    // go until bad character, or end of data
    size_t startPosition = position;
    if (IsPunctuation(contents[position]))
    {
        while (position < size && IsPunctuation(contents[position]))
            position++;
    }
    else
    {
        while (position < size && IsAlphaNumeric(contents[position]))
            position++;
    }

    // copy the word into the string
    word = std::string(&contents[startPosition], &contents[position]);

    // make lowercase for consistency
    std::transform(word.begin(), word.end(), word.begin(), ::tolower);
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
        if (!nextWord.empty())
            g_markovChain.Observe(lastWord, nextWord);
    }

    return true;
}

bool GenerateStatsFile(const char* fileName)
{
    FILE* file = nullptr;
    fopen_s(&file, fileName, "w+t");
    if (!file)
        return false;

    // show the data we have
    fprintf(file, "\n\nWord Counts:\n");
    for (auto& wordCounts : g_markovChain.m_counts)
    {
        fprintf(file, "[+] %s\n", wordCounts.first.c_str());

        for (auto& wordCount : wordCounts.second)
            fprintf(file, "[++] %s - %zu\n", wordCount.first.c_str(), wordCount.second);
    }

    fprintf(file, "\n\nWord Probabilities:\n");
    for (auto& wordCounts : g_markovChain.m_probabilities)
    {
        fprintf(file, "[-] %s\n", wordCounts.first.c_str());

        float lastProbability = 0.0f;
        for (auto& wordCount : wordCounts.second)
        {
            fprintf(file, "[--] %s - %i%%\n", wordCount.observed.c_str(), int((wordCount.cumulativeProbability - lastProbability)*100.0f));
            lastProbability = wordCount.cumulativeProbability;
        }
    }

    // show the ignored letters
    fprintf(file, "\n\nIgnored Letters:\n");
    for (int i = 1; i < 256; ++i)
    {
        if (IsAlphaNumeric(char(i)) || IsPunctuation(char(i)))
            continue;

        fprintf(file, "%c (%i)\n", char(i), i);
    }

    return true;
}

bool GenerateFile(const char* fileName, size_t wordCount)
{
    FILE* file = nullptr;
    fopen_s(&file, fileName, "w+t");
    if (!file)
        return false;

    // get the initial starting state
    std::string word = g_markovChain.GetInitialState();
    std::string lastWord = word;
    word[0] = toupper(word[0]);
    fprintf(file, "%s", word.c_str());

    bool capitalizeFirstLetter = false;

    for (size_t wordIndex = 0; wordIndex < wordCount; ++wordIndex)
    {
        std::string nextWord = g_markovChain.GetNextState(lastWord);
        lastWord = nextWord;

        if (capitalizeFirstLetter)
        {
            nextWord[0] = toupper(nextWord[0]);
            capitalizeFirstLetter = false;
        }

        if (nextWord == "." || nextWord == "," || nextWord == ";" || nextWord == ":")
            fprintf(file, "%s", nextWord.c_str());
        else
            fprintf(file, " %s", nextWord.c_str());

        if (nextWord == ".")
            capitalizeFirstLetter = true;
    }

    fclose(file);
    return true;
}

int main(int argc, char** argv)
{
    std::vector<const char*> inputFiles =
    {
        //"data/projbluenoise.txt",
        //"data/psychreport.txt",
        "data/lastquestion.txt",
        "data/telltale.txt",
    };

    // process input
    for (const char* inputFile : inputFiles)
    {
        printf("processing %s...\n", inputFile);
        if (!ProcessFile(inputFile))
        {
            printf("could not open file %s!\n", inputFile);
            return 1;
        }
    }

    // make probabilities
    printf("Calculating probabilities...\n");
    g_markovChain.FinalizeLearning();

    // make statistics file
    if (!GenerateStatsFile("out/stats.txt"))
    {
        printf("Could not generate stats file!\n");
        return 1;
    }

    // make output
    if (!GenerateFile("out/generated.txt", 1000))
    {
        printf("Could not generate output file!\n");
        return 1;
    }

    return 0;
}

/*

TODO:

* Nth order! just make the key string be the appending of the last two words maybe?
* then write blog post.

Next: try with images?
 * maybe just have a "N observed states = M possible outputs" general markov chain. Maybe try it with more than images? letters? audio? i dunno

Note: all sorts of copies and ineficiencies in code. Runs fast enough for this usage case, and was fast to write, so good enough.

*/