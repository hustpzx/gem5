#!/usr/bin/python
import xlsxwriter
import collections

benches = ['basicmath', 'blowfish','crc','patricia',
    'rsynth','typeset', 'stringsearch']

key_list = [
    'sim_seconds',
    'sim_ticks',
    'system.cpu.op_class::MemRead',
    'system.cpu.op_class::MemWrite',

    'system.memories.bytes_read::total',
    'system.memories.bytes_written::total'
]

dict = collections.OrderedDict()

workbook = xlsxwriter.Workbook('sram.xlsx')
worksheet = workbook.add_worksheet()

fisrtColumn = 0

# pure-sram scheme leakage power (mW)
leakagePower = 113.781 * 1.5

for j, bench in enumerate(benches):
    path = bench + '/small_stats.txt'

    # open & read the stats files
    with open(path, 'r') as f: lines = f.readlines()
    print('open: ' + path)

# write the row head(bench list) to sheet
    worksheet.write(0, j+1, bench)

    # extract data from stats file and record to a dict
    for k, key in enumerate(key_list):
        # write the column head(key list) to sheet
        if(fisrtColumn == 0):
            worksheet.write(k+1, 0, key)

        value = '0'
        for line in lines:
            if(key in line):
                value = line.split()[1]
                dict[key] = value
    fisrtColumn = 1

    # write bench stats data to xlsx file
    for n, v in enumerate(dict.values()):
        worksheet.write(n+1, j+1, v)

    staticEnergy = leakagePower * float(dict["sim_seconds"]) * 1000000

    length = len(key_list) +5
    worksheet.write(length, 0, "StaticEnergy")
    worksheet.write(length, j+1, staticEnergy)

    # access energy per bit, unit:nJ
    sram_readEnergy = 0.117
    sram_writeEnergy = 0.094

    dynamicEnergy = ((int(dict['system.memories.bytes_read::total'])) * \
        sram_readEnergy
        + (int(dict['system.memories.bytes_written::total'])) * \
        sram_writeEnergy ) * 8
    length = length + 2
    worksheet.write(length, 0, "dynamicEnergy")
    worksheet.write(length, j+1, dynamicEnergy)

    totalEnergy = staticEnergy + dynamicEnergy
    length = length + 2
    worksheet.write(length, 0, "totalEnergy")
    worksheet.write(length, j+1, totalEnergy)

workbook.close()
print("PURE-SRAM stats data extracting finished")
