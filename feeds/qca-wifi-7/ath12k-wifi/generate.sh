#!/bin/sh

# https://github.com/qca/qca-swiss-army-knife.git

encoder=/code/qca/qca-swiss-army-knife/tools/scripts/ath12k/ath12k-bdencoder

$encoder -c board-2-eap105-IPQ5332.json -o board-2.bin.eap105.IPQ5332
$encoder -c board-2-eap105-QCN92XX.json -o board-2.bin.eap105.QCN92XX

$encoder -c board-2-rap7110c_341x-IPQ5332.json -o board-2.bin.rap7110c_341x.IPQ5332
$encoder -c board-2-rap7110c_341x-QCN92XX.json -o board-2.bin.rap7110c_341x.QCN92XX

$encoder -c board-2-ap72tip-IPQ5332.json -o board-2.bin.ap72tip.IPQ5332
$encoder -c board-2-ap72tip-QCN92XX.json -o board-2.bin.ap72tip.QCN92XX

$encoder -c board-2-ap72tip-v4-IPQ5332.json -o board-2.bin.ap72tip-v4.IPQ5332
$encoder -c board-2-ap72tip-v4-QCN92XX.json -o board-2.bin.ap72tip-v4.QCN92XX
