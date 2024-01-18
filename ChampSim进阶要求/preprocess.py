def process_file(input_file, output_file):
    with open(input_file, 'r') as file:
        lines = file.readlines()
    
    new_lines = []
    skip_next_line = False
    for i, line in enumerate(lines):
        # 如果标记为跳过这一行（因为它已经被合并），则跳过
        if skip_next_line:
            skip_next_line = False
            continue
        
        # 检查当前行是否包含 'Hit Rate' 但不是百分比行
        if 'Hit Rate' in line and '%' not in line:
            # 合并当前行和下一行
            combined_line = line.strip() + ' ' + lines[i + 1]
            new_lines.append(combined_line)
            skip_next_line = True  # 下一行已合并，所以跳过
        else:
            new_lines.append(line)  # 不需要合并，直接添加当前行
    
    # 将处理后的内容写入输出文件
    with open(output_file, 'w') as file:
        file.writelines(new_lines)

# 使用函数处理文件
process_file('allresult.txt', 'processed_result.txt')

