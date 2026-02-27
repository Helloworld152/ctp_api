#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
比较JSON和CSV文件中的合约，找出JSON中多出的合约
筛选条件：!expired && (product_class == Futures || Options || FOption)
"""

import csv
import json
import sys
from collections import defaultdict

def extract_instrument_id(key):
    """
    从JSON的key中提取合约代码
    格式可能是：
    - KQ.i@CFFEX.IF -> CFFEX.IF
    - CFFEX.HO2301-C-2325 -> HO2301-C-2325
    - DCE.SP a2603&a2605 -> SP a2603&a2605
    """
    if '.' in key:
        parts = key.split('.', 1)
        if len(parts) == 2:
            inst_part = parts[1]
            # 如果包含@，提取@后面的部分
            if '@' in inst_part:
                inst_part = inst_part.split('@', 1)[1]
            return inst_part
    return key

def is_valid_product_class(class_type):
    """
    判断class是否符合筛选条件
    kProductClassFutures -> 'FUTURE'
    kProductClassOptions -> 'OPTION'
    kProductClassFOption -> 'FUTURE_OPTION'
    
    只接受这三种精确匹配，不包括：
    - FUTURE_COMBINE (组合)
    - FUTURE_INDEX (指数)
    - FUTURE_CONT (连续)
    """
    if not class_type:
        return False
    
    # 精确匹配三种类型
    return class_type in ['FUTURE', 'OPTION', 'FUTURE_OPTION']

def main():
    # 文件路径
    csv_file = 'instruments_20260204_152938.csv'
    json_file = 'latest_ins_cache.json'
    output_file = 'extra_instruments.txt'
    
    print('=== 开始处理 ===')
    
    # 1. 读取CSV文件中的合约代码
    print(f'读取CSV文件: {csv_file}')
    csv_instruments = set()
    try:
        # 先尝试正常读取
        with open(csv_file, 'r', encoding='utf-8', errors='ignore') as f:
            reader = csv.DictReader(f)
            for row in reader:
                if '合约代码' in row and row['合约代码']:
                    # 去除可能的空字符和空白
                    inst_code = row['合约代码'].strip().replace('\x00', '')
                    if inst_code:
                        csv_instruments.add(inst_code)
        print(f'  CSV中的合约数: {len(csv_instruments)}')
    except FileNotFoundError:
        print(f'错误: 找不到CSV文件 {csv_file}')
        sys.exit(1)
    except csv.Error as e:
        print(f'错误: CSV格式错误: {e}')
        # 尝试用更宽松的方式读取
        print('尝试用备用方法读取...')
        try:
            with open(csv_file, 'r', encoding='utf-8', errors='ignore') as f:
                lines = f.readlines()
                # 跳过表头
                for line in lines[1:]:
                    if line.strip():
                        # 简单分割，取第一列
                        parts = line.split(',')
                        if parts and parts[0].strip():
                            inst_code = parts[0].strip().replace('\x00', '')
                            if inst_code:
                                csv_instruments.add(inst_code)
            print(f'  CSV中的合约数（备用方法）: {len(csv_instruments)}')
        except Exception as e2:
            print(f'错误: 备用方法也失败: {e2}')
            sys.exit(1)
    except Exception as e:
        print(f'错误: 读取CSV文件失败: {e}')
        sys.exit(1)
    
    # 2. 读取并筛选JSON文件
    print(f'读取JSON文件: {json_file}')
    try:
        with open(json_file, 'r', encoding='utf-8') as f:
            data = json.load(f)
        print(f'  JSON中的总合约数: {len(data)}')
    except FileNotFoundError:
        print(f'错误: 找不到JSON文件 {json_file}')
        sys.exit(1)
    except Exception as e:
        print(f'错误: 读取JSON文件失败: {e}')
        sys.exit(1)
    
    # 3. 筛选JSON中的合约
    print('筛选JSON中的合约...')
    filtered_instruments = {}  # key -> instrument_id
    stats = {
        'total': 0,
        'expired': 0,
        'not_expired': 0,
        'invalid_class': 0,
        'passed_filter': 0
    }
    
    for key, value in data.items():
        stats['total'] += 1
        
        if not isinstance(value, dict):
            continue
        
        # 检查是否过期
        expired = value.get('expired', True)
        if expired:
            stats['expired'] += 1
            continue
        
        stats['not_expired'] += 1
        
        # 检查产品类型
        class_type = value.get('class', '')
        if not is_valid_product_class(class_type):
            stats['invalid_class'] += 1
            continue
        
        # 通过筛选
        stats['passed_filter'] += 1
        instrument_id = extract_instrument_id(key)
        filtered_instruments[key] = instrument_id
    
    print(f'  总合约数: {stats["total"]}')
    print(f'  已过期: {stats["expired"]}')
    print(f'  未过期: {stats["not_expired"]}')
    print(f'  产品类型不符合: {stats["invalid_class"]}')
    print(f'  通过筛选的key数量: {stats["passed_filter"]}')
    
    # 4. 找出JSON中多出的合约
    print('对比合约列表...')
    filtered_instrument_set = set(filtered_instruments.values())
    extra_instruments = filtered_instrument_set - csv_instruments
    
    print(f'  JSON筛选后的key数量: {len(filtered_instruments)}')
    print(f'  JSON筛选后的合约代码数（去重后）: {len(filtered_instrument_set)}')
    print(f'  JSON中多出的合约数: {len(extra_instruments)}')
    
    # 5. 统计多出合约的信息
    if extra_instruments:
        print('\n统计多出合约的信息...')
        extra_by_exchange = defaultdict(list)
        extra_by_class = defaultdict(list)
        
        for key, inst_id in filtered_instruments.items():
            if inst_id in extra_instruments:
                # 提取交易所
                if '.' in key:
                    exchange = key.split('.', 1)[0]
                else:
                    exchange = '未知'
                extra_by_exchange[exchange].append(inst_id)
                
                # 提取class
                if key in data and isinstance(data[key], dict):
                    class_type = data[key].get('class', '未知')
                    extra_by_class[class_type].append(inst_id)
        
        print(f'\n按交易所分类:')
        for exchange in sorted(extra_by_exchange.keys()):
            print(f'  {exchange}: {len(extra_by_exchange[exchange])}个')
        
        print(f'\n按产品类型分类:')
        for class_type in sorted(extra_by_class.keys()):
            print(f'  {class_type}: {len(extra_by_class[class_type])}个')
    
    # 6. 写入输出文件
    print(f'\n写入输出文件: {output_file}')
    try:
        with open(output_file, 'w', encoding='utf-8') as f:
            # 写入统计信息
            f.write(f'# JSON筛选后多出的合约列表\n')
            f.write(f'# 筛选条件: !expired && (class == FUTURE || class == OPTION || class == FUTURE_OPTION)\n')
            f.write(f'# JSON筛选后的key数量: {len(filtered_instruments)}\n')
            f.write(f'# JSON筛选后的合约代码数（去重后）: {len(filtered_instrument_set)}\n')
            f.write(f'# CSV中的合约数: {len(csv_instruments)}\n')
            f.write(f'# 多出的合约数: {len(extra_instruments)}\n')
            f.write(f'#\n')
            
            # 写入合约代码（按字母顺序排序）
            for inst_id in sorted(extra_instruments):
                f.write(f'{inst_id}\n')
        
        print(f'  成功写入 {len(extra_instruments)} 个合约代码')
    except Exception as e:
        print(f'错误: 写入文件失败: {e}')
        sys.exit(1)
    
    print(f'\n=== 处理完成 ===')
    print(f'结果已保存到: {output_file}')

if __name__ == '__main__':
    main()
