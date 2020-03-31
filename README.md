# wasm-miner
This is the repo for 14828/18636 WebAssembly Ethereum miner project. 

## Current progress
`miner.html, miner.js and miner.wasm` are compiled through `emcc --bind miner.cpp -o miner.html -s ALLOW_MEMORY_GROWTH=1`.

Currently it runs in terminal with `node miner.js` but no output will be displayed when opening `miner.html`.

All input are dummy content and it is not really mining for now.

## Future plan (TODO)
Fix the problem that the html file will not display the output.

Get the miner work, probably through `geth` (Go Ethereum, as listed in the reference section). We can construct an central server with `geth`, which will send current block information to our clients(`local-miner`) for mining.

Also we can try other simpler cryptocurrencies (e.g. Zcash) if possible.

## Useful references
[Ethminer](https://github.com/ethereum-mining/ethminer): a C++ Ethereum miner supporting GPU mining

[Go Ethereum](https://github.com/ethereum/go-ethereum): the official Go implementation of the Ethereum protocol, support CPU mining and private network

[Bitcoin cpuminer](https://github.com/jgarzik/cpuminer): a multi-threaded CPU miner for Bitcoin

[A Monero WebAssembly based miner](https://github.com/jtgrassie/xmr-wasm): as title
