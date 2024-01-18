#!/bin/bash

# 创建或清空allresult.txt文件
echo "" >allresult.txt

# 函数：计算并打印命中率
calculate_and_print_hit_rate() {
	local hits=$1
	local misses=$2

	# 如果总访问次数为0，则避免除以零
	if [ $((hits + misses)) -eq 0 ]; then
		echo "Hit Rate: N/A (No accesses)"
	else
		local hit_rate=$(echo "scale=2; $hits / ($hits + $misses) * 100" | bc)
		echo "Hit Rate: $hit_rate%"
	fi
}

# 循环遍历所有.txt文件
for file in result/*.txt; do
	# 确保文件存在
	if [ ! -f "$file" ]; then
		continue
	fi

	# 在allresult.txt中输出当前处理的文件名
	echo "Processing file: $file" >>allresult.txt

	# 逐行读取文件并处理
	while IFS= read -r line || [[ -n "$line" ]]; do
		# 输出前两行内容
		if [[ $line == Core_0_IPC* ]] || [[ $line == Core_0_branch_prediction_accuracy* ]]; then
			echo "$line" >>allresult.txt
		fi

		# 对于hit和miss行，计算命中率
		if [[ $line == Core_0_*_total_hit* ]]; then
			hits=$(echo "$line" | awk '{print $2}')
			metric_prefix=$(echo "$line" | awk -F '_' '{print $1 "_" $2 "_" $3}')

			# 读取下一行以获取miss值
			read -r next_line
			misses=$(echo "$next_line" | awk '{print $2}')

			# 计算并输出命中率
			echo "$metric_prefix Hit Rate:" >>allresult.txt
			calculate_and_print_hit_rate "$hits" "$misses" >>allresult.txt
		fi
	done <"$file"

	# 在allresult.txt中添加分隔符
	echo "--------------------------------" >>allresult.txt
done

echo "所有文件处理完成。结果保存在 allresult.txt 中。"
