#!/usr/bin/python
import xlsxwriter
import collections

benches = ['envmnt', 'smart-home', 'thermodetector']

key_list = [
    'sim_seconds',
    'sim_ticks',

    'system.cpu.op_class::MemRead',
    'system.cpu.op_class::MemWrite',

    'system.umc.ec_nmReads',
    'system.umc.ec_nmWrites',
    'system.umc.ec_fmReads',
    'system.umc.ec_fmWrites',
    'system.umc.ec_bkpReads',
    'system.umc.ec_bkpWrites',

    'system.umc.ec_highState',
    'system.umc.ec_middleState',
    'system.umc.ec_lowState',

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

workbook = xlsxwriter.Workbook('econ_stats.xlsx')
worksheet = workbook.add_worksheet()

fisrtColumn = 0

# hybrid scheme leakage power(mW), contains 256KB SRAM & 2.25MB STTRAM
LP_low = 5.093 + 227.562 / 4
LP_middle = 5.093 + 227.562 / 2
LP_high = 5.093 + 227.562

for j, bench in enumerate(benches):
    s_path = bench + '/econ_stats.txt'

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

    # different state time in tick
    highTime = (int(dict["system.umc.ec_highState"]) + 1) * 1000000000
    middleTime = int(dict["system.umc.ec_middleState"]) * 1000000000
    lowTime = int(dict["sim_tick"]) - highTime - middleTime

    # 10^12 tick = 1s , 1 mW*s = 1000000 J
    static_high = LP_high * (highTime / 1000000000000) * 1000000
    static_middle = LP_middle * (middleTime / 1000000000000) * 1000000
    static_low = LP_low * (lowTime / 1000000000000) * 1000000

    staticEnergy = static_low + static_middle + static_high

    length = len(key_list) +5
    worksheet.write(length, 0, "StaticLow")
    worksheet.write(length, j+1, static_low)
    length += 2
    worksheet.write(length, 0, "StaticMiddle")
    worksheet.write(length, j+1, static_middle)
    length += 2
    worksheet.write(length, 0, "StaticHigh")
    worksheet.write(length, j+1, static_high)
    length += 2
    worksheet.write(length, 0, "StaticEnergy")
    worksheet.write(length, j+1, staticEnergy)

    # access energy per bit, unit:nJ
    fm_readEnergy = 0.103
    fm_writeEnergy = 1.1
    nm_readEnergy = 0.117
    nm_writeEnergy = 0.094

    memDynamic = \
        (int(dict["system.memories0.bytes_read::total"]) * fm_readEnergy
        + int(dict["system.memories0.bytes_written::total"]) * fm_writeEnergy
        + int(dict["system.memories1.bytes_read::total"]) * nm_readEnergy
        + int(dict["system.memories1.bytes_written::total"]) * nm_writeEnergy \
        ) * 8

    extraDynamic =(int(dict["system.umc.extra_fm_reads"]) * fm_readEnergy
        + int(dict["system.umc.extra_fm_writes"]) * fm_writeEnergy
        + int(dict["system.umc.extra_nm_reads"]) * nm_readEnergy
        + int(dict["system.umc.extra_nm_writes"]) * nm_writeEnergy ) \
        * 8

    EC_Dynamic = (int(dict["system.umc.ec_fmReads"]) * fm_readEnergy
        + int(dict["system.umc.ec_fmWrites"]) * fm_writeEnergy
        + int(dict["system.umc.ec_nmReads"]) * nm_readEnergy
        + int(dict["system.umc.ec_nmWrites"]) * nm_writeEnergy
        + int(dict["system.umc.ec_bkpReads"]) * fm_readEnergy
        + int(dict["system.umc.ec_bkpWrites"]) * fm_writeEnergy ) \
        * 8

    length = length + 2
    worksheet.write(length, 0, "extraDynamicEnergy")
    worksheet.write(length, j+1, extraDynamic)

    length += 2
    worksheet.write(length, 0, "EC_Dynamic")
    worksheet.write(length, j+1, EC_Dynamic)

    totalDynamic = memDynamic + extraDynamic + EC_Dynamic
    length = length + 2
    worksheet.write(length, 0, "totalDynamicEnergy")
    worksheet.write(length, j+1, totalDynamic)

    totalEnergy = staticEnergy + totalDynamic
    length = length + 2
    worksheet.write(length, 0, "totalEnergy")
    worksheet.write(length, j+1, totalEnergy)



workbook.close()
print("Hybrid stats data extracting finished")
