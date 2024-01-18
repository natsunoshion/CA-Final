#!/bin/bash

# 设定预热和模拟指令数
WARMUP_INST=1
SIM_INST=5

# 获取当前目录的绝对路径
DIR="$(pwd)"

# 寻找所有bin文件（不限制扩展名）
BIN_FILES=($(find "${DIR}/bin" -type f))

# 寻找所有trace文件
TRACE_FILES=($(find "${DIR}/traces" -type f))

# 函数：提取指定的性能指标
extract_metric() {
	grep "$1" | awk '{print $2}'
}

# 循环遍历所有bin和trace文件的组合
for BIN_FILE in "${BIN_FILES[@]}"; do
	for TRACE_FILE in "${TRACE_FILES[@]}"; do
		# 提取文件名用于输出文件
		BIN_NAME=$(basename "${BIN_FILE}")
		TRACE_NAME=$(basename "${TRACE_FILE}")

		# 构建输出文件名
		OUTPUT_FILE="result/${BIN_NAME}_${TRACE_NAME}.txt"

		# 执行并重定向输出到文件
		echo "运行：$BIN_FILE 与 $TRACE_FILE"
		OUTPUT=$(./run_champsim.sh "${BIN_FILE}" $WARMUP_INST $SIM_INST "${TRACE_FILE}")

		# 提取并处理输出
		{
			echo "Core_0_IPC: $(echo "$OUTPUT" | extract_metric "Core_0_IPC")"
			echo "Core_0_branch_prediction_accuracy: $(echo "$OUTPUT" | extract_metric "Core_0_branch_prediction_accuracy")"
			echo "Core_0_L1D_total_hit: $(echo "$OUTPUT" | extract_metric "Core_0_L1D_total_hit")"
			echo "Core_0_L1D_total_miss: $(echo "$OUTPUT" | extract_metric "Core_0_L1D_total_miss")"
			echo "Core_0_L1I_total_hit: $(echo "$OUTPUT" | extract_metric "Core_0_L1I_total_hit")"
			echo "Core_0_L1I_total_miss: $(echo "$OUTPUT" | extract_metric "Core_0_L1I_total_miss")"
			echo "Core_0_L2C_total_hit: $(echo "$OUTPUT" | extract_metric "Core_0_L2C_total_hit")"
			echo "Core_0_L2C_total_miss: $(echo "$OUTPUT" | extract_metric "Core_0_L2C_total_miss")"
			echo "Core_0_LLC_total_hit: $(echo "$OUTPUT" | extract_metric "Core_0_LLC_total_hit")"
			echo "Core_0_LLC_total_miss: $(echo "$OUTPUT" | extract_metric "Core_0_LLC_total_miss")"
		} >"${OUTPUT_FILE}"

		echo "${BIN_NAME}_${TRACE_NAME}.txt输出完成"
	done
done

echo "所有模拟完成。"
