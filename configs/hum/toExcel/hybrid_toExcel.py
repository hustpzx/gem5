#!/usr/bin/python
import xlsxwriter
import collections

benches = ['basicmath','blowfish','crc','patricia',
         'rsynth','typeset', 'stringsearch']

key_list = [
    'sim_seconds',
    'sim_ticks',
    'system.cpu.op_class::MemRead',
    'system.cpu.op_class::MemWrite',

    'system.umc.extra_fm_reads',
    'system.umc.extra_fm_writes',
    'system.umc.extra_nm_reads',
    'system.umc.extra_nm_writes',

    'system.memories0.bytes_read::total',
    'system.memories0.bytes_written::total',
    'system.memories1.bytes_read::total',
    'system.memories1.bytes_written::total',

    'system.umc.extraTimeConsumption'

]

dict = collections.OrderedDict()

workbook = xlsxwriter.Workbook('hyrbid.xlsx')
worksheet = workbook.add_worksheet()

fisrtColumn = 0

# hybrid scheme leakage power(mW), contains SRAM & STTRAM
leakagePower = 4.093 + 113.781

for j, bench in enumerate(benches):
    s_path = bench + '/small_stats.txt'

    # open & read the stats files
    with open(s_path, 'r') as f: lines = f.readlines()
    print('open: ' + s_path)

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
    fm_readEnergy = 0.103
    fm_writeEnergy = 1.1
    nm_readEnergy = 0.117
    nm_writeEnergy = 0.094

    memDynamic = (int(dict["system.memories0.bytes_read::total"]) * \
        fm_readEnergy
        + int(dict["system.memories0.bytes_written::total"]) * fm_writeEnergy
        + int(dict["system.memories1.bytes_read::total"]) * nm_readEnergy
        + int(dict["system.memories1.bytes_written::total"]) * \
        nm_writeEnergy) * 8

    extraDynamic =(int(dict["system.umc.extra_fm_reads"]) * fm_readEnergy
        + int(dict["system.umc.extra_fm_writes"]) * fm_writeEnergy
        + int(dict["system.umc.extra_nm_reads"]) * nm_readEnergy
        + int(dict["system.umc.extra_nm_writes"]) * nm_writeEnergy ) \
        * 8

    length = length + 2
    worksheet.write(length, 0, "extraDynamicEnergy")
    worksheet.write(length, j+1, extraDynamic)

    totalDynamic = memDynamic + extraDynamic
    length = length + 2
    worksheet.write(length, 0, "totalDynamicEnergy")
    worksheet.write(length, j+1, totalDynamic)

    totalEnergy = staticEnergy + totalDynamic
    length = length + 2
    worksheet.write(length, 0, "totalEnergy")
    worksheet.write(length, j+1, totalEnergy)



workbook.close()
print("Hybrid stats data extracting finished")
