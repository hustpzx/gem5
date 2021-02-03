#!/usr/bin/python
import xlsxwriter
import collections

benches = ['basicmath','blowfish','crc','patricia',
    'rsynth','typeset', 'stringsearch']

key_list = [
    'host_inst_rate',
    'host_seconds',
    'sim_seconds',
    'sim_ticks',
    'system.cpu.op_class::MemRead',
    'system.cpu.op_class::MemWrite',

    'system.cache.readHits',
    'system.cache.writeHits',
    'system.cache.misses',

    'system.memories.bytes_read::total',
    'system.memories.bytes_written::total',
]

dict = collections.OrderedDict()

workbook = xlsxwriter.Workbook('twolevel.xlsx')
worksheet = workbook.add_worksheet()

fisrtColumn = 0

# twolevel scheme leakage power(mW)
leakagePower = 4.093 + 113.781

for j, bench in enumerate(benches):
    path = bench + '/small_stats.txt'

    # open & read the stats files
    with open(path, 'r') as f: lines = f.readlines()
    print('open: ' + path)

    # write the fisrt row(bench_list) to sheet
    worksheet.write(0, j+1, bench)

    # extract data from stats file and record to a dict
    for k, key in enumerate(key_list):
        # write the first column(key_list) to sheet
        if(fisrtColumn==0):
            worksheet.write(k+1, 0 , key)

        found = 0
        for line in lines:
            if(key in line):
                value = line.split()[1]
                dict[key] = value
                found = 1
        if(found == 0):
            dict[key] = '0'

    fisrtColumn = 1

     # write bench stats data to xlsx file
    for n, v in enumerate(dict.values()):
        worksheet.write(n+1, j+1, v)

    staticEnergy = leakagePower * float(dict["sim_seconds"]) * 1000000

    length = len(key_list) +5
    worksheet.write(length, 0, "StaticEnergy")
    worksheet.write(length, j+1, staticEnergy)

    # access energy per bit, unit:nJ
    fm_readEnergy = 0.103
    fm_writeEnergy = 1.1
    nm_readEnergy = 0.117
    nm_writeEnergy = 0.094

    cacheDynamic = ((int(dict['system.cache.readHits'])) * nm_readEnergy * 4
        + (int(dict['system.cache.writeHits'])) * nm_writeEnergy * 4 ) *8
    length = length + 2
    worksheet.write(length, 0, "cacheDynamic")
    worksheet.write(length, j+1, cacheDynamic)

    memDynamic =
    ((int(dict['system.memories.bytes_read::total'])) * fm_readEnergy
    + (int(dict['system.memories.bytes_written::total'])) * fm_writeEnergy ) \
        * 8
    length = length + 2
    worksheet.write(length, 0, "memDynamic")
    worksheet.write(length, j+1, memDynamic)

    totalDynamic = memDynamic + cacheDynamic
    length = length + 2
    worksheet.write(length, 0, "totalDynamic")
    worksheet.write(length, j+1, totalDynamic)

    totalEnergy = staticEnergy + totalDynamic
    length = length + 2
    worksheet.write(length, 0, "totalEnergy")
    worksheet.write(length, j+1, totalEnergy)

workbook.close()
print("TwoLevel stats data extracting finished")
