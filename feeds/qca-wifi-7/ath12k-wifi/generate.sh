#!/bin/sh

encoder=/code/qca/qca-swiss-army-knife/tools/scripts/ath12k/ath12k-bdencoder

$encoder -c board-2-eap105-IPQ5332.json -o board-2.bin.eap105.IPQ5332
$encoder -c board-2-eap105-QCN92XX.json -o board-2.bin.eap105.QCN92XX
