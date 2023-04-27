# Seeds

Utility to generate [src/chainparamsseeds.h](/src/chainparamsseeds.h), used to hard code some notes into Riecoin Core. Manually add known reliable and cooperative nodes to nodes_main.txt (MainNet) and nodes_test.txt (TestNet), then run

    python3 generate-seeds.py . > ../../src/chainparamsseeds.h
