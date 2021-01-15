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
    'system.silc.agingResetNum',
    'system.silc.queryNum',
    'system.silc.swapNum',
    'system.memories0.bytes_read::.cpu.inst',
    'system.memories0.bytes_read::.cpu.data',
    'system.memories0.bytes_read::total',
    'system.memories0.bytes_written::total',
    'system.memories1.bytes_read::.cpu.inst',
    'system.memories1.bytes_read::.cpu.data',
    'system.memories1.bytes_read::total',
    'system.memories1.bytes_written::total',
]


workbook = xlsxwriter.Workbook('silc.xlsx')
worksheet = workbook.add_worksheet()

fisrtColumn = 0

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

        worksheet.write(k+1, j+1, value)

    fisrtColumn = 1

workbook.close()
print("PURE stats data extracting finished")
