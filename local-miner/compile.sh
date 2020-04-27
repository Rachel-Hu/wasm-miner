emcc \
--bind miner.cpp \
-o miner.html \
-g4 \
-O3 \
-s EXIT_RUNTIME=1 \
-s ALLOW_MEMORY_GROWTH=1 \
-s ASYNCIFY \
-s EXPORTED_FUNCTIONS='["_main", "_processCommand"]' \
-s EXTRA_EXPORTED_RUNTIME_METHODS='["ccall", "cwrap"]' \
--source-map-base http://localhost:42001/wasm-miner/local-miner/

#-s TOTAL_MEMORY=1089863680 \
#-s WASM_MEM_MAX=1089863680
#-s USE_PTHREADS=1 \
