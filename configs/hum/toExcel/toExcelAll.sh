#!/usr/bin/

cd ./hybrid
echo "Extract Hybrid stats file"
./toExcel.py

cd ../imo
echo "Extract IMO stats file"
./toExcel.py

cd ../pure
echo "Extract PURE-STTRAM stats file"
./toExcel.py

cd ../silc
echo "Extract SILC stats file"
./toExcel.py

cd ../sram
echo "Extract SRAM stats file"
./toExcel.py

cd ../twolevel
echo "Extract TwoLevel stats file"
./toExcel.py



