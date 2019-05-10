#include <stdio.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <random>
#include <algorithm>
#include <array>

// array hasher
namespace std
{
    template<typename T, size_t N>
    struct hash<array<T, N> >
    {
        typedef array<T, N> argument_type;
        typedef size_t result_type;

        result_type operator()(const argument_type& a) const
        {
            hash<T> hasher;
            result_type h = 0;
            for (result_type i = 0; i < N; ++i)
            {
                h = h * 31 + hasher(a[i]);
            }
            return h;
        }
    };
}

template <typename TType, size_t ORDER_N>
class MarkovChain
{
public:

    MarkovChain()
        : m_rd("dev/random")
        , m_fullSeed{ m_rd(), m_rd(), m_rd(), m_rd(), m_rd(), m_rd(), m_rd(), m_rd() }
        , m_rng(m_fullSeed)
    {

    }

    typedef std::array<TType, ORDER_N> Observations;

    typedef std::unordered_map<TType, size_t> TObservedCounts;

    struct ObservedProbability
    {
        TType observed;
        float cumulativeProbability;
    };

    typedef std::vector<ObservedProbability> TObservedProbabilities;

    struct ObservationContext
    {
        ObservationContext()
        {
            std::fill(m_hasObservation.begin(), m_hasObservation.end(), false);
        }

        Observations              m_observations;
        std::array<bool, ORDER_N> m_hasObservation;
    };

    ObservationContext GetObservationContext()
    {
        return ObservationContext();
    }

    // learning stage
    void RecordObservation(ObservationContext& context, const TType& next)
    {
        // if this observation has a full set of data observed, record this observation
        if (context.m_hasObservation[ORDER_N - 1])
            m_counts[context.m_observations][next]++;

        // move all observations down
        for (size_t index = ORDER_N - 1; index > 0; --index)
        {
            context.m_observations[index] = context.m_observations[index - 1];
            context.m_hasObservation[index] = context.m_hasObservation[index - 1];
        }

        // put in the new observation
        context.m_observations[0] = next;
        context.m_hasObservation[0] = true;
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
    Observations GetInitialObservations()
    {
        // select a starting state entirely at random
        std::uniform_int_distribution<size_t> dist(0, m_probabilities.size());
        size_t index = dist(m_rng);
        auto it = m_probabilities.begin();
        std::advance(it, index);
        return it->first;
    }

    void GetNextObservations(Observations& observations)
    {
        // get the next state by choosing a weighted random next state.
        std::uniform_real_distribution<float> distFloat(0.0f, 1.0f);
        TObservedProbabilities& probabilities = m_probabilities[observations];
        if (probabilities.size() == 0)
            return;

        float nextStateProbability = distFloat(m_rng);
        int nextStateIndex = 0;
        while (nextStateIndex < probabilities.size() - 1 && probabilities[nextStateIndex].cumulativeProbability < nextStateProbability)
            ++nextStateIndex;

        // move all observations down
        for (size_t index = ORDER_N - 1; index > 0; --index)
            observations[index] = observations[index - 1];

        // put the new observation in
        observations[0] = probabilities[nextStateIndex].observed;
    }

    // file output
    void fprintf(FILE* file, const Observations& observations)
    {
        for (int i = 0; i < ORDER_N; ++i)
        {
            if (i == 0)
                ::fprintf(file, "%s", observations[i].c_str());
            else
                ::fprintf(file, " %s", observations[i].c_str());
        }
    }

    void fprintf(FILE* file, const std::vector<ObservedProbability>& observations)
    {
        for (int i = 0; i < observations.size(); ++i)
        {
            if (i == 0)
                ::fprintf(file, "%s", observations[i].c_str());
            else
                ::fprintf(file, " %s", observations[i].c_str());
        }
    }

    // random number generation storage
    std::random_device m_rd;
    std::seed_seq m_fullSeed;
    std::mt19937 m_rng;

    // data storage
    std::unordered_map<Observations, TObservedCounts> m_counts;
    std::unordered_map<Observations, TObservedProbabilities> m_probabilities;
};

MarkovChain<std::string, 2> g_markovChain;
MarkovChain<std::string, 2> g_markovChain2ndOrder;

// TODO: hook g_markovChain2ndOrder up

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

bool GetWord(unsigned char* contents, size_t size, size_t& position, std::string& word)
{
    // skip ignored characters to start
    while (position < size && !IsAlphaNumeric(contents[position]) && !IsPunctuation(contents[position]))
        position++;

    // exit if there is no word
    if (position >= size)
    {
        word = "";
        return false;
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

    return true;
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

    // get an observation context
    auto context = g_markovChain.GetObservationContext();

    // process the file
    size_t position = 0;
    size_t size = contents.size();
    std::string nextWord;
    while(GetWord(contents.data(), size, position, nextWord))
        g_markovChain.RecordObservation(context, nextWord);

    return true;
}

bool GenerateStatsFile(const char* fileName)
{
    FILE* file = nullptr;
    fopen_s(&file, fileName, "w+t");
    if (!file)
        return false;

    // show the data we have
    fprintf(file, "\n\nWord Counts:");
    for (auto& wordCounts : g_markovChain.m_counts)
    {
        fprintf(file, "\n[+] ");
        g_markovChain.fprintf(file, wordCounts.first);
        fprintf(file, "\n");

        for (auto& wordCount : wordCounts.second)
            fprintf(file, "[++] %s - %zu\n", wordCount.first.c_str(), wordCount.second);
    }

    fprintf(file, "\n\nWord Probabilities:");
    for (auto& wordCounts : g_markovChain.m_probabilities)
    {
        fprintf(file, "\n[-] ");
        g_markovChain.fprintf(file, wordCounts.first);
        fprintf(file, "\n");

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

template <typename MARKOVCHAIN>
bool GenerateFile(const char* fileName, size_t wordCount, MARKOVCHAIN& markovChain)
{
    FILE* file = nullptr;
    fopen_s(&file, fileName, "w+t");
    if (!file)
        return false;

    // get the initial starting state
    auto observations = markovChain.GetInitialObservations();
    std::string word = observations[0];
    word[0] = toupper(word[0]);
    fprintf(file, "%s", word.c_str());

    bool capitalizeFirstLetter = false;

    for (size_t wordIndex = 0; wordIndex < wordCount; ++wordIndex)
    {
        markovChain.GetNextObservations(observations);

        std::string nextWord = observations[0];
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
        "data/projbluenoise.txt",
        "data/psychreport.txt",
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
    if (!GenerateFile("out/generated.txt", 1000, g_markovChain))
    {
        printf("Could not generate output file!\n");
        return 1;
    }

    return 0;
}

/*

TODO:


? what to do when you encounter something without anything to transition to (probabilities size is zero for what to go to next).
 ? we could avoid them by removing them from the data when finalizing. Maybe that's appropriate?

* Nth order! just make the key string be the appending of the last two words maybe?
* then write blog post.

* put ellipses at the start and end of the generated file

Next: try with images?
 * maybe just have a "N observed states = M possible outputs" general markov chain. Maybe try it with more than images? letters? audio? i dunno

Note: all sorts of copies and ineficiencies in code. Runs fast enough for this usage case, and was fast to write, so good enough.

*/