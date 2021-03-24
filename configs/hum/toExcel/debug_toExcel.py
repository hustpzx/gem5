#!/usr/bin/python
import xlsxwriter
import collections

workbook = xlsxwriter.Workbook('debug.xlsx')
worksheet = workbook.add_worksheet()

path = 'debug.txt'
with open(path, 'r') as f:lines = f.readlines()
print('open file!')

n = 0
for line in lines:
    if('umc' in line):
        cntr = line.split()[2]
        worksheet.write(0,n, cntr)
        n += 1

workbook.close()
print("extract finished!")