#!/usr/bin/python
import xlsxwriter
import collections

workbook = xlsxwriter.Workbook('sh-memStatus.xlsx')
worksheet = workbook.add_worksheet()

path = 'MemStatus.txt'
with open(path, 'r') as f:lines = f.readlines()
print('open file!')

n = 0
m = 0
for line in lines:
    if('ioCntr' in line):
        cntr = line.split()[3]
        worksheet.write(n,0, cntr)
        n += 1

    if('Status' in line):
        status = line.split()[3]
        worksheet.write(m, 2, status)
        m += 1



workbook.close()
print("extract finished!")
