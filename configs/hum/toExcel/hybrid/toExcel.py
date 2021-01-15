#!/usr/bin/python
import xlsxwriter
import collections

benches = ['basicmath','blowfish','crc','patricia',
         'rsynth','typeset', 'stringsearch']
length = len(benches)
# since all stats files have same format which means a line number relates to
# a parameter, so we use line number as index directly to avoid string finding
# or pattern matching.

# host_inst_rate                    3
# host_seconds                      6
# sim_seconds                       11
# sim_ticks                         12
# cpu.MemRead                       205
# cpu.MemWrite                      206
# umc.extra_fm_reads                213
# umc.extra_fm_writes               214
# umc.extra_nm_reads                215
# umc.extra_nm_writes               216
# umc.migrations                    217
# memories0.bytes_read::cpu.inst    229
# memories0.bytes_read::cpu.data    230
# memories0.byetes_read::total      231
# memories0.bytes_written::total    235
# memories1.bytes_read::cpu.inst    252
# memories1.bytes_read::cpu.data    253
# memories1.bytes_read::total       254
# memories1.bytes_written::total    258

index_list = [3,6,11,12, 205,206,  229,230,231,235,\
    252,253,254,258,  213,214,215,216,217]
#length = len(index_list)
s_dict = collections.OrderedDict()
l_dict = collections.OrderedDict()

workbook = xlsxwriter.Workbook('hyrbid.xlsx')
worksheet = workbook.add_worksheet()

check_key = []
flag = 0

for j, bench in enumerate(benches):
    s_path = bench + '/small_stats.txt'
    #l_path = bench + '/large_stats.txt'

    # open & read the stats files
    with open(s_path, 'r') as sf: slines = sf.readlines()
    print('open: ' + s_path)
    #with open(l_path, 'r') as lf: llines = lf.readlines()
    #print('open: ' + l_path)

    # extract data from stats file and record to a dict
    a = 0
    for i in index_list:
        sline = slines[i]
        skey = sline.split()[0]
        svalue = sline.split()[1]
        s_dict[skey] = svalue
        """
        lline = llines[i]
        lkey = lline.split()[0]
        lvalue = lline.split()[1]
        l_dict[lkey] = lvalue
        """
        if(flag == 0):
            check_key.append(skey)
        else:
            if(skey != check_key[a]):
                print("Non-matchable attribute ")
                exit(0)
            a += 1

    if(flag == 0):
        # add attribute list to xlsx file
        for m, key in enumerate(check_key):
            #print(key)
            worksheet.write(m+1, 0, key)

    flag = 1

    # write bench stats data to xlsx file
    worksheet.write(0, j+1, bench)
    for n, v in enumerate(s_dict.values()):
        worksheet.write(n+1, j+1, v)
    """
    worksheet.write(0, j+length+5, bench)
    for n, v in enumerate(l_dict.values()):
        worksheet.write(n+1, j+length+5, v)
    """

workbook.close()
print("Hybrid stats data extracting finished")
