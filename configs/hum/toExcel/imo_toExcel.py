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

    'system.cpu.icache.overall_hits::total',
    'system.cpu.icache.overall_misses::total',
    'system.cpu.icache.overall_accesses::total',
    'system.cpu.icache.ReadReq_hits::total',
    'system.cpu.icache.ReadReq_misses::total',

    'system.cpu.dcache.overall_hits::total',
    'system.cpu.dcache.overall_misses::total',
    'system.cpu.dcache.overall_accesses::total',
    'system.cpu.dcache.ReadReq_hits::total',
    'system.cpu.dcache.ReadReq_misses::total',
    'system.cpu.dcache.WriteReq_hits::total',
    'system.cpu.dcache.WriteReq_misses::total',

    'system.mem_ctrl.bytes_read::.cpu.inst',
    'system.mem_ctrl.bytes_read::.cpu.data',
    'system.mem_ctrl.bytes_read::total',
    'system.mem_ctrl.bytes_written::total',
]

dict = collections.OrderedDict()

workbook = xlsxwriter.Workbook('IMO.xlsx')
worksheet = workbook.add_worksheet()

fisrtColumn = 0

# IMO scheme leakage power(mW) ,contains SRAM, DRAM and STTRAM
leakagePower = 113.781 + 54.237 + 4.093

# refresh power of DRAM(mW)
refreshPower = 0.02

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

        value = '0'
        for line in lines:
            if(key in line):
                value = line.split()[1]
                dict[key] = value

    fisrtColumn = 1

    # write stats data to sheet
    for n, v in enumerate(dict.values()):
        worksheet.write(n+1, j+1, v)

    staticEnergy = leakagePower * float(dict['sim_seconds']) * 1000000
    length = len(key_list) +5
    worksheet.write(length, 0, "StaticEnergy")
    worksheet.write(length, j+1, staticEnergy)

    refreshEnergy = refreshPower * float(dict['sim_seconds']) * 1000000
    length = length + 2
    worksheet.write(length, 0, "RefreshEnergy")
    worksheet.write(length, j+1, refreshEnergy)

    # access energy per bit, unit:nJ
    sttram_readEnergy = 0.103
    sttram_writeEnergy = 1.1
    dram_readEnergy = 0.318
    dram_writeEnergy = 0.351
    sram_readEnergy = 0.117
    sram_writeEnergy = 0.094


    dynamicEnergy = ((int(dict['system.cpu.icache.ReadReq_hits::total'])) * \
        sram_readEnergy * 64
        + (int(dict['system.cpu.dcache.ReadReq_hits::total'])) * \
        dram_readEnergy * 64
        + (int(dict['system.cpu.dcache.WriteReq_hits::total'])) * \
        dram_writeEnergy * 64
        + (int(dict['system.mem_ctrl.bytes_read::total'])) * \
        sttram_readEnergy
        + (int(dict['system.mem_ctrl.bytes_written::total'])) * \
        sttram_writeEnergy ) * 8

    length = length + 2
    worksheet.write(length, 0, "dynamicEnergy")
    worksheet.write(length, j+1, dynamicEnergy)

    totalEnergy = staticEnergy + refreshEnergy + dynamicEnergy
    length = length + 2
    worksheet.write(length, 0, "totalEnergy")
    worksheet.write(length, j+1, totalEnergy)




workbook.close()
print("IMO stats data extracting finished")
