#!/usr/bin/

cd ./hybrid
echo "Extract Hybrid stats file"
./hybrid_toExcel.py

cd ../imo
echo "Extract IMO stats file"
./imo_toExcel.py

cd ../pure
echo "Extract PURE-STTRAM stats file"
./pure_toExcel.py

cd ../silc
echo "Extract SILC stats file"
./silc_toExcel.py

cd ../sram
echo "Extract SRAM stats file"
./sram_toExcel.py

cd ../twolevel
echo "Extract TwoLevel stats file"
./twolevel_toExcel.py



