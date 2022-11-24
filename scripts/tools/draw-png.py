#!/usr/bin/env python3

import matplotlib.pyplot as plt
import matplotlib.ticker as ticker 
import json
import unicodedata
import argparse
import os
import numpy as np

class Record:
    def __init__(self, data):
        self._isvalid = True
        (self._category, name) = data["name"].split('/')
        (self._encode, self._algo) = name.split('_')
        self._cputime = data["cpu_time"]
        self._time_unit = data["time_unit"]
        if "label" in data:
            self._label = data["label"]
        else:
            self._isvalid = False

    def file(self):
        return self._file

    def category(self):
        return self._category
    
    def is_valid(self):
        return self._isvalid

    def label(self):
        if self._isvalid:
            return self._label
        return ""

    def algo(self):
        return self._algo

    def is_encode(self):
        return self._encode == "Encode"

    def is_decode(self):
        return self._encode == "Decode"

    def cputime(self):
        return self._cputime

    def dump(self):
        print("%s: %s %s %f %s"%(self._label, self._algo, self._encode, self._cputime, self._time_unit))

def parse_json(json_file):
    with open(json_file, 'r') as f:
        data = json.load(f)
        benchmarks = data["benchmarks"]
        records = []
        for benchmark in benchmarks:
            record = Record(benchmark)
            records.append(record)

    tests = {}
    for record in records:
        if record.is_decode() or record.is_encode():
            if record.category() not in tests:
                tests[record.category()] = [record]
            else:
                tests[record.category()].append(record)

    return tests

def to_percent(temp, position):
    return '%1.0f'%(100*temp) + '%'

def draw_text(bar, fontsize):
    csfont = {'fontname':'Comic Sans MS'}
    hfont = {'fontname':'Helvetica'}
    for rect in bar:
        height = rect.get_height()
        if height == 0:
            text = "N/A"
        else:
            text = '%.f%%' % (height*100)
        plt.text(rect.get_x() + rect.get_width() / 2.0, height, text, ha='center', va='bottom', **{'size' : fontsize})

def draw_png(data1, data2, xnames, title, label1, label2, fontsize, png):
    print("Generating PNG %s ..." %(png))
    width = 0.4
    x = list(range(len(data1)))

    bar1 = plt.bar(x, data1, width = width, label = label1, facecolor = 'steelblue')
    for i in range(len(x)):
        x[i] = x[i] + width
    bar2 = plt.bar(x, data2, width = width, label = label2, facecolor = 'darkorange', tick_label = xnames)

    draw_text(bar1, fontsize)
    draw_text(bar2, fontsize)    

    plt.title(title)
    
    plt.gca().yaxis.set_major_formatter(ticker.FuncFormatter(to_percent))
    plt.xticks(size = fontsize)
 
    plt.rc('legend', fontsize=fontsize) 
    plt.rc('ytick', labelsize=fontsize)
    plt.legend()
    plt.savefig(png, dpi = 200)
    plt.cla()

def get_percentage(data):
    percentage = []
    for i in range(len(data)):
        if i == 0:
            percentage.append(1.0)
        elif data[0].cputime():
            percentage.append(data[i].cputime()/data[0].cputime())
        else:
            percentage.append(0.0)
    return percentage

def draw_one_png(group, png, overall):
    encode = []
    decode = []
    for record in group:
        if record.is_encode():
            encode.append(record)
        else:
            decode.append(record)
    encode.sort(key=lambda x : x.algo())
    decode.sort(key=lambda x : x.algo())
    
    encode_percentage = get_percentage(encode)
    decode_percentage = get_percentage(decode)

    for i in range(len(encode)):
        if encode[i].algo() not in overall:
            overall[encode[i].algo()] = ([(encode[i].cputime(), encode_percentage[i])], [])
        else:
            if encode_percentage[i]:
                overall[encode[i].algo()][0].append((encode[i].cputime(), encode_percentage[i]))

    for i in range(len(decode)):
        if decode[i].algo() not in overall:
            overall[decode[i].algo()] = ([], [(decode[i].cputime(), decode_percentage[i])])
        else:
            if decode_percentage[i]:
                overall[decode[i].algo()][1].append((decode[i].cputime(), decode_percentage[i]))

    if len(encode) != len(decode):
        print("Mismatch encode/decode size")
        return

    assert (len(encode_percentage) == len(decode_percentage))

    x_names = []
    title = None
    category = None
    for i in range(len(encode)):
        if encode[i].algo() != decode[i].algo():
            print("Mismatch encode/decode algo")
            return
        x_names.append(encode[i].algo())
        if i == 0:
            category = encode[i].category()
            title = encode[i].label()
        elif title != encode[i].label() or category != encode[i].category():
            print ("Mismatch encode/decode category")
            return
        
    draw_png(encode_percentage, decode_percentage, x_names, title, "Encode", "Decode", 8, png)

def wide_chars(s):
    return sum(unicodedata.east_asian_width(x) =='W' for x in s)
def str_width(s):
    return len(s) + wide_chars(s)

def draw_in_text(encode_percentage, decode_percentage, encode_cputime, decode_cputime, names):
    yellow = "\x1b[33;21m"
    reset = "\x1b[0m"
    blue = "\x1b[0;34m"
    green = "\x1b[32;21m"
    red = "\x1b[31;21m"

    padding = 8
    fields_width = []
    headers = ["Names", "Encode", "Encode Avg Time(ns)", "Decode", "Decode Avg Time(ns)"]
    for header in headers:
        fields_width.append(str_width(header) + padding)
    
    name_width = fields_width[0]
    for name in names:
        name_width = max(name_width, str_width(name) + padding)

    fields_width[0] = max(fields_width[0], name_width)
    fields_width[len(fields_width) - 1] = fields_width[len(fields_width) - 1] - padding

    dash_width = sum(fields_width)
    
    print("")
    print ("-"*(dash_width))
    for i in range(len(headers)):
        print(blue + "%-*s" % (fields_width[i], headers[i]) + reset, end='')
    print ("")
    print ("-"*(dash_width))

    for i in range(len(names)):
        print(green + "%-*s" %(fields_width[0], names[i]) + reset, end='')
        v = 'N/A'
        if i < len(encode_percentage):
            v = '%.2f%%'%(encode_percentage[i] * 100)
        
        print(yellow + "%-*s" % (fields_width[1], v), end='')

        v = 'N/A'
        if i < len(encode_cputime):
            v = '%.f' % (encode_cputime[i])

        print(yellow + "%-*s" % (fields_width[2], v), end='')

        v = 'N/A'
        if i < len(decode_percentage):
            v = '%.2f%%'%(decode_percentage[i] * 100)

        print("%-*s" % (fields_width[3], v), end ='')
    
        v = 'N/A'
        if i < len(decode_cputime):
            v = '%.f' % (decode_cputime[i])

        print(yellow + "%-*s" % (fields_width[4], v), end='')
        print("")
    print(reset)


def draw_overall_png(overall, png):
    encode_overall_percentage = []
    decode_overall_percentage = []
    encode_overall = []
    decode_overall = []
    x_names = []
    for (algo, (v1, v2)) in overall.items():
        x_names.append(algo)
        total_cputime = 0.0
        total_percentage = 0.0
        for (encode_cputime, encode_percentage) in v1:
            total_cputime = total_cputime + encode_cputime
            total_percentage = total_percentage + encode_percentage
        
        if len(v1):
            encode_overall.append(total_cputime / len(v1))
            encode_overall_percentage.append(total_percentage / len(v1))
        
        total_cputime = 0.0
        total_percentage = 0.0

        for (encode_cputime, encode_percentage) in v2:
            total_cputime = total_cputime + encode_cputime
            total_percentage = total_percentage + encode_percentage
        
        if len(v2):
            decode_overall.append(total_cputime / len(v2))
            decode_overall_percentage.append(total_percentage / len(v2))

    if len(encode_overall_percentage) == len(decode_overall_percentage):
        draw_png(encode_overall_percentage, decode_overall_percentage, x_names,
            "Overall", "Encode", "Decode", 5, png)

    draw_in_text(encode_overall_percentage, decode_overall_percentage,
        encode_overall, decode_overall, x_names)

def sort_by_labels(groups, labels):
    d = {}
    for (key, values) in groups.items():
        y = []
        for k in labels:
            if values.get(k) is None:
                print(k) # error
            y.append(values[k])
        d[key] = y
    return d

def normalize(groups):
    sonic = groups["SonicDyn"]
    d = {}
    for (k, v) in groups.items():
        d[k] = [ x / y for x, y in zip(sonic, v)]

    return d

def process_multibar_data(groups, labels):
    groups = sort_by_labels(groups, labels)
    return normalize(groups)

def draw_compare_png(groups, png):
    algos_encode = {}
    algos_decode = {}
    labels = list(groups.keys())
    for (k, records) in groups.items():
        for record in records:
            if record.is_encode():
                if algos_encode.get(record.algo()) is None:
                    algos_encode[record.algo()] = {}
                algos_encode[record.algo()][k] = record.cputime()
            elif record.is_decode():
                if algos_decode.get(record.algo()) is None:
                    algos_decode[record.algo()] = {}
                algos_decode[record.algo()][k] = record.cputime()

    if algos_encode.get("SonicDyn") is None or algos_decode.get("SonicDyn") is None:
        print("No sonic data!!!")
        return

    draw_multibar_png(labels, process_multibar_data(algos_encode, labels), "Encode", png)
    draw_multibar_png(labels, process_multibar_data(algos_decode, labels), "Decode", png)

def draw_multibar_png(labels, algos, title, png):
    x = np.arange(len(labels))  # the label locations
    width = 0.6 / len(algos)  # the width of the bars
    bar_mid = -0.3 + width / 2

    fig, ax = plt.subplots()
    for (algo, y) in algos.items():
        ax.bar(x + bar_mid, y, width, label = algo)
        bar_mid += width

    ax.set_ylabel('perforamnce')
    ax.set_title(title + ' performance comparsion (HIB)')
    ax.set_xticks(x, labels)
    ax.legend()

    for label in ax.get_xmajorticklabels():
        label.set_rotation(30)
        label.set_horizontalalignment("right")

    plt.tight_layout()
    plt.savefig(png + '_' + title + ".png", dpi = 200)
    plt.cla()

def main():
    argparser = argparse.ArgumentParser(description='Tools to draw png for a testrun')
    argparser.add_argument('json', metavar = 'test-result.json')
    args = argparser.parse_args()

    groups = parse_json(args.json)
    overall = {}

    for (json, group) in groups.items():
        if len(group):
            png = "docs/images/%s.png"%(group[0].category())
            draw_one_png(group, png, overall)

    draw_compare_png(groups, "docs/images/compare")
    draw_overall_png(overall, "docs/images/overall.png")

if __name__ == "__main__":
    main()
