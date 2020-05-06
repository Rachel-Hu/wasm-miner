# wasm-miner
This is the repo for 14828/18636 WebAssembly Ethereum miner project. 

## Current progress
`miner.html, miner.js and miner.wasm` are compiled by executing `./compile.sh` in the local-miner directory.

Runs in terminal by calling `node miner.js` or by opening `miner.html` in a browser. If opening in a browser, the HTML file must be served by a web server; the HTML file cannot be loaded as a local filesystem file (file://). 

All input are dummy content and it is not really mining for now.

## Future plan (TODO)
Get the miner work, probably through `geth` (Go Ethereum, as listed in the reference section). We can construct an central server with `geth`, which will send current block information to our clients(`local-miner`) for mining.

Also we can try other simpler cryptocurrencies (e.g. Zcash) if possible.

## Citations
The miner simulated the Keccak hash function utilized by Ethereum.`js/ethash.js` in this repo, [ethash](https://github.com/ethereum/ethash), is a important reference of the hashing code implementation of `local-miner/miner.cpp`.

## Useful references
[Ethash](https://github.com/ethereum/ethash): PoW algorithm code for Etherium 1.0

[Ethminer](https://github.com/ethereum-mining/ethminer): a C++ Ethereum miner supporting GPU mining

[Go Ethereum](https://github.com/ethereum/go-ethereum): the official Go implementation of the Ethereum protocol, support CPU mining and private network

[Bitcoin cpuminer](https://github.com/jgarzik/cpuminer): a multi-threaded CPU miner for Bitcoin

[A Monero WebAssembly based miner](https://github.com/jtgrassie/xmr-wasm): as title
