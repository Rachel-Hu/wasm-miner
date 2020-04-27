/*
 * Reference for hashing/mining code:
 * https://github.com/ethereum/ethash/blob/master/js/ethash.js
 */

#include <time.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <sstream>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <fstream>
#include "miner.h"

#include <emscripten.h>

unsigned int *dag;
unsigned int numSlicesLocal;
unsigned int cacheHit = 0;
unsigned int numAccesses = 0;

class Params
{
public:
	unsigned int cacheSize;
	unsigned int cacheRounds;
	unsigned int dagSize;
	unsigned int dagParents;
	unsigned int mixSize;
	unsigned int mixParents;

	Params()
	{
		this->cacheSize = 1048384;
		this->cacheRounds = 3;
		this->dagSize = 1073739904;
		this->dagParents = 256;
		this->mixSize = 128;
		this->mixParents = 64;
	}

	Params(unsigned int cacheSize, unsigned int dagSize)
	{
		this->cacheSize = cacheSize;
		this->cacheRounds = 3;
		this->dagSize = dagSize;
		this->dagParents = 256;
		this->mixSize = 128;
		this->mixParents = 64;
	}
};

class Keccak
{
public:
	// changed stateBuf from uchar to uint
	// buffer was 200 bytes long, so 50 ints
	unsigned int stateBuf[50];
	unsigned int *stateWords;
	Keccak()
	{
		// got rid of stateBytes
		// don't need 2 datastructures pointing to the same object
		this->stateWords = this->stateBuf;
	}

	void digestWords(unsigned int *oWords, unsigned int oOffset,
					 unsigned int oLength, unsigned int *iWords, unsigned int iOffset,
					 unsigned int iLength)
	{
		for (unsigned int i = 0; i < 50; ++i)
			this->stateWords[i] = 0;

		unsigned int r = 50 - oLength * 2;
		while (true)
		{
			unsigned int len = iLength < r ? iLength : r;
			for (unsigned int i = 0; i < len; ++i, ++iOffset)
				this->stateWords[i] ^= iWords[iOffset];

			if (iLength < r)
				break;
			iLength -= len;

			keccak_f1600(this->stateWords, 0, 50, this->stateWords);
		}

		// converted byte level operations to word level operations
		this->stateWords[iLength] ^= 1;
		this->stateWords[r - 1] ^= 0x80000000;

		keccak_f1600(oWords, oOffset, oLength, this->stateWords);
	}
};

void store(std::string dagStr, unsigned int startIndex, unsigned int endIndex)
{
	std::stringstream ss(dagStr);
	unsigned int start = startIndex * 16;
	unsigned int end = endIndex * 16;
	if (end > numSlicesLocal * 16)
		end = numSlicesLocal * 16;
	for (unsigned int i = start; i < end; i += 16)
		for (unsigned int j = 0; j < 16; j++)
			ss >> dag[i + j];
}

void cacheComputeSliceStore(unsigned int nodeIndex, unsigned int *node)
{
	unsigned int index = nodeIndex * 16;
	if (index + 16 >= numSlicesLocal * 16)
		return;
	for (unsigned int j = 0; j < 16; j++)
		dag[index + j] = node[j];
}

unsigned int *DAGLookup(unsigned int index)
{
	unsigned int i = index * 16;
	bool present = false;
	if (i + 16 >= numSlicesLocal * 16)
		return NULL;
	for (unsigned int w = 0; w < 16; w++)
	{
		if (dag[i + w] != 0)
			present = true;
	}
	if (!present)
		return NULL;
	return &dag[i];
}

unsigned int fnv(unsigned int x, unsigned int y)
{
	// js integer multiply by 0x01000193 will lose precision
	return x * 0x01000193 ^ y;
}

void computeDagNode(unsigned int *o_node, Params *params, unsigned int *cache, Keccak *keccak, unsigned int nodeIndex)
{
	unsigned int cacheNodeCount = params->cacheSize >> 6;
	unsigned int dagParents = params->dagParents;

	unsigned int c = (nodeIndex % cacheNodeCount) << 4;
	unsigned int *mix = o_node;

	for (unsigned int w = 0; w < 16; ++w)
		mix[w] = cache[c | w];

	mix[0] ^= nodeIndex;

	keccak->digestWords(mix, 0, 16, mix, 0, 16);

	for (unsigned int p = 0; p < dagParents; ++p)
	{
		// compute cache node (word) index
		c = (fnv(nodeIndex ^ p, mix[p & 15]) % cacheNodeCount) << 4;
		for (unsigned int w = 0; w < 16; ++w)
			mix[w] = fnv(mix[w], cache[c | w]);
	}

	keccak->digestWords(mix, 0, 16, mix, 0, 16);
	cacheComputeSliceStore(nodeIndex, o_node);
}

void computeHashInner(unsigned int *mix, Params *params, unsigned int *cache, Keccak *keccak, unsigned int *tempNode)
{
	unsigned int mixParents = params->mixParents;
	unsigned int mixWordCount = params->mixSize >> 2;
	unsigned int mixNodeCount = mixWordCount >> 4;
	unsigned int dagPageCount = params->dagSize / 32;

	// grab initial first word
	unsigned int s0 = mix[0];

	// initialise mix from initial 64 bytes
	for (unsigned int w = 16; w < mixWordCount; ++w)
		mix[w] = mix[w & 15];

	for (unsigned int a = 0; a < mixParents; ++a)
	{
		unsigned int p = fnv(s0 ^ a, mix[a & (mixWordCount - 1)]) % dagPageCount;
		unsigned int d = p * mixNodeCount;
		for (unsigned int n = 0, w = 0; n < mixNodeCount; n++, w = w + 16)
		{
			numAccesses++;
			if (DAGLookup(d + n) != NULL)
			{
				cacheHit++;
				tempNode = DAGLookup(d + n);
			}
			else
				computeDagNode(tempNode, params, cache, keccak, d + n);

			for (unsigned int v = 0; v < 16; ++v)
				mix[w | v] = fnv(mix[w | v], tempNode[v]);
		}
	}
}

class Ethash
{
public:
	Params *params;
	unsigned int *cache;
	// changed unsigned char to uint, now size of initBuf is 96/4 = 24
	unsigned int *mixWords;
	unsigned int tempNode[16];
	Keccak *keccak;
	unsigned int retWords[8];
	unsigned int initWords[24];

	Ethash(Params *params, unsigned int cache[])
	{
		this->params = params;
		this->cache = cache;
		// preallocate buffers/etc

		// got rid of initBytes and retBytes
		// don't need 2 datastructures pointing to the same buffer.
		for (unsigned int i = 0; i < 24; i++)
			this->initWords[i] = 0;
		this->mixWords = new unsigned int[this->params->mixSize / 4];
		this->keccak = new Keccak();
	}

	unsigned int *hash(unsigned int *header, unsigned int *nonce)
	{
		// compute initial hash

		// TO DO: use header hash instead of header
		//checked from the javascript version of the miner that the header size is always 32 bytes (8 ints)

		// the header
		for (unsigned int i = 0; i < 8; i++)
			// changed initBytes to initWords
			this->initWords[i] = header[i];
		// we know nonce is always 8 uint8_t elements (64 bit nonce)
		for (unsigned int i = 8; i < 10; i++)
			// changed initBytes to initWords
			this->initWords[i] = nonce[i - 8];

		this->keccak->digestWords(this->initWords, 0, 16, this->initWords, 0, 10);

		// compute mix
		for (unsigned int i = 0; i != 16; i++)
			this->mixWords[i] = this->initWords[i];

		computeHashInner(this->mixWords, this->params, this->cache, this->keccak, this->tempNode);

		// compress mix and append to initWords
		// note: this->params->mixSize / 4 = mixwords.length
		for (unsigned int i = 0; i != this->params->mixSize / 4; i = i + 4)
			this->initWords[16 + i / 4] = fnv(fnv(fnv(this->mixWords[i], this->mixWords[i + 1]), this->mixWords[i + 2]), this->mixWords[i + 3]);

		// final Keccak hashes
		this->keccak->digestWords(this->retWords, 0, 8, this->initWords, 0, 24); // Keccak-256(s + cmix)
		return this->retWords;
	}
};

void deserialize(std::string str, unsigned int *outArr, unsigned int size)
{
	std::stringstream ss(str);
	unsigned int i = 0;
	while (ss.good() && i < size)
	{
		ss >> outArr[i];
		++i;
	}
}

double mine(std::string headerStr, std::string cacheStr, std::string dagStr,
			unsigned int startIndex, unsigned int endIndex, unsigned int cacheSize,
			unsigned int dagSize)
{

	// the hash must be less than the following for the nonce to be a valid solutions
	srand(time(NULL));
	Params params(cacheSize, dagSize);
	unsigned int header[8];
	numSlicesLocal = 10000; // 16777186
	unsigned int *cache = new unsigned int[cacheSize];
	//printf("setting dag to int array of size %d\n", numSlicesLocal * 16);
	dag = new unsigned int[numSlicesLocal * 16]();

	deserialize(headerStr, header, 8);
	deserialize(cacheStr, cache, cacheSize);
	store(dagStr, startIndex, endIndex);

	Ethash hasher(&params, cache);
	unsigned int nonce[] = {0, 0};
	unsigned int trials = 450000; // 4500000;
	unsigned int *hash;

	// timing the hashes
	std::chrono::high_resolution_clock::time_point start;
	std::chrono::high_resolution_clock::time_point stop;
	std::chrono::duration<double, std::milli> time;
	double hashRate;
	int interval = 50000;
	int sleepinterval = 1000;

	start = std::chrono::high_resolution_clock::now();

	for (int i = 0; i < trials; i++)
	{
		hash = hasher.hash(header, nonce);
		nonce[rand() % 2] = rand() % ((unsigned int)0xffffffff);
		if (i % interval == 0)
		{
			stop = std::chrono::high_resolution_clock::now();
			time = stop - start;
			hashRate = 1000.0 * interval / (time.count());
			//printf("c %d:  %f\n", i, ((float)cacheHit / (float)numAccesses));
			printf("hash #%d:  %f hashes per second\n", i, hashRate);

			cacheHit = 0;
			numAccesses = 0;
			start = std::chrono::high_resolution_clock::now();
		}
		if (i % sleepinterval == 0) {
			emscripten_sleep(100); // sleep to allow screen to refresh
		}
	}

	return hashRate;
}

extern "C" {
	void sayHi() {
		emscripten_run_script("minebot.connection.send('Hello!')");
	}

	void processCommand(char *message) {
		//printf("Received command: %s\n", message);

		const char delim[2] = " ";
		char *token;

		token = strtok(message, delim);

		if (strcmp(token, "sayhi") == 0) {
			sayHi();

		} else if (strcmp(token, "say") == 0) {
			char script[1024];
			sprintf(script, "minebot.connection.send('%s')", message+strlen("say "));
			emscripten_run_script(script);

		} else if (strcmp(token, "echotoconsole") == 0) {
			printf("%s\n", message+strlen("echotoconsole "));

		} else if (strcmp(token, "alert") == 0) {
			char script[1024];
			sprintf(script, "alert('%s')", message+strlen("alert "));
			emscripten_run_script(script);

		} else if (strcmp(token, "execute") == 0) {
			emscripten_run_script(message+strlen("execute "));
		}

	}
}

void main_loop()
{
	int ret = 0;
	double rate = 0;

	std::ifstream t("dag_hex.txt");
	std::stringstream buffer;
	buffer << t.rdbuf();

	while (true)
	{
		rate = mine("387d4d41", "8f1e678b", buffer.str(), 0, 1000, 1024, 1024);
		printf("Client average hashrate: %f\n", rate);
		emscripten_sleep(1000); // sleep to allow screen to refresh
	}
}

int main()
{
	printf("Starting...\n");
	emscripten_sleep(300);
	emscripten_set_main_loop(main_loop, 60, true);
	return 0;
}
