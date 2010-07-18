#include <iostream>
#include <vector>
#include <utility>
#include <tr1/unordered_map>

#include <boost/functional/hash.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/lexical_cast.hpp>

#include "tdict.h"
#include "fdict.h"
#include "wordid.h"
#include "filelib.h"

using namespace std;
using namespace std::tr1;
namespace po = boost::program_options;

static const size_t MAX_LINE_LENGTH = 100000;
WordID kBOS, kEOS, kDIVIDER, kGAP;
int kCOUNT;

void InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("input,i", po::value<string>()->default_value("-"), "Input file")
        ("phrases,p", po::value<string>(), "File contatining phrases of interest")
        ("phrase_context_size,S", po::value<int>()->default_value(2), "Use this many words of context on left and write when writing base phrase contexts")
        ("silent", "Write nothing to stderr except errors")
        ("help,h", "Print this help message and exit");
  po::options_description clo("Command line options");
  po::options_description dcmdline_options;
  dcmdline_options.add(opts);

  po::store(parse_command_line(argc, argv, dcmdline_options), *conf);
  po::notify(*conf);

  if (conf->count("help") || conf->count("input") != 1 || conf->count("phrases") != 1) {
    cerr << "\nUsage: extractor_monolingual [-options]\n";
    cerr << dcmdline_options << endl;
    exit(1);
  }
}

struct TrieNode
{
  TrieNode(int l) : finish(false), length(l) {};
  ~TrieNode()
  {
    for (unordered_map<int, TrieNode*>::iterator
         it = next.begin(); it != next.end(); ++it)
      delete it->second;
    next.clear();
  }

  TrieNode *follow(int token)
  {
    unordered_map<int, TrieNode*>::iterator
      found = next.find(token);
    if (found != next.end())
      return found->second;
    else
      return 0;
  }

  void insert(const vector<int> &tokens)
  {
    insert(tokens.begin(), tokens.end());
  }

  void insert(vector<int>::const_iterator begin, vector<int>::const_iterator end)
  {
    if (begin == end)
      finish = true;
    else
    {
      int token = *begin;
      unordered_map<int, TrieNode*>::iterator 
        nit = next.find(token);
      if (nit == next.end())
        nit = next.insert(make_pair(token, new TrieNode(length+1))).first;
      ++begin;
      nit->second->insert(begin, end);
    }
  }

  bool finish;
  int length;
  unordered_map<int, TrieNode*> next;
};

void WriteContext(const vector<int>& sentence, int start, int end, int ctx_size) 
{
  for (int i = start; i < end; ++i)
  {
    if (i != start) cout << " ";
    cout << sentence[i];
  }
  cout << '\t';
  for (int i = ctx_size; i > 0; --i)
    cout << TD::Convert(sentence[start-i]) << " ";
  cout << " " << TD::Convert(kGAP);
  for (int i = 0; i < ctx_size; ++i)
    cout << " " << TD::Convert(sentence[end+i]);
  cout << "\n";
}

inline bool IsWhitespace(char c) { 
    return c == ' ' || c == '\t'; 
}

inline void SkipWhitespace(const char* buf, int* ptr) {
  while (buf[*ptr] && IsWhitespace(buf[*ptr])) { ++(*ptr); }
}

vector<int> ReadSentence(const char *buf, int padding)
{
  int ptr = 0;
  SkipWhitespace(buf, &ptr);
  int start = ptr;
  vector<int> sentence;
  for (int i = 0; i < padding; ++i)
    sentence.push_back(kBOS);

  while (char c = buf[ptr])
  {
    if (!IsWhitespace(c)) 
      ++ptr; 
    else {
      sentence.push_back(TD::Convert(string(buf, start, ptr-start)));
      SkipWhitespace(buf, &ptr);
      start = ptr;
    }
  }
  for (int i = 0; i < padding; ++i)
    sentence.push_back(kEOS);

  return sentence;
}

int main(int argc, char** argv) 
{
  po::variables_map conf;
  InitCommandLine(argc, argv, &conf);
  kBOS = TD::Convert("<s>");
  kEOS = TD::Convert("</s>");
  kDIVIDER = TD::Convert("|||");
  kGAP = TD::Convert("<PHRASE>");
  kCOUNT = FD::Convert("C");

  bool silent = conf.count("silent") > 0;
  const int ctx_size = conf["phrase_context_size"].as<int>();

  char buf[MAX_LINE_LENGTH];
  TrieNode phrase_trie(0);
  ReadFile rpf(conf["phrases"].as<string>());
  istream& pin = *rpf.stream();
  while (pin) {
      pin.getline(buf, MAX_LINE_LENGTH);
      phrase_trie.insert(ReadSentence(buf, 0));
  }

  ReadFile rif(conf["input"].as<string>());
  istream &iin = *rif.stream();
  int line = 0;
  while (iin) {
    ++line;
    iin.getline(buf, MAX_LINE_LENGTH);
    if (buf[0] == 0) continue;
    if (!silent) {
      if (line % 200 == 0) cerr << '.';
      if (line % 8000 == 0) cerr << " [" << line << "]\n" << flush;
    }

    vector<int> sentence = ReadSentence(buf, ctx_size);
    vector<TrieNode*> tries(1, &phrase_trie);
    for (int i = ctx_size; i < (int)sentence.size() - ctx_size; ++i)
    {
      vector<TrieNode*> tries_prime(1, &phrase_trie);
      for (vector<TrieNode*>::iterator tit = tries.begin(); tit != tries.end(); ++tit)
      {
        TrieNode* next = (*tit)->follow(sentence[i]);
        if (next != 0)
        {
          if (next->finish)
            WriteContext(sentence, i - next->length, i, ctx_size);
          tries_prime.push_back(next);
        }
      }
      swap(tries, tries_prime);
    }
  }
  if (!silent) cerr << endl;
  return 0;
}
