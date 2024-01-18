#!/bin/bash

# 每个参数的选定范围
# 分支预测器的选项
# 这里没有提供分支预测器的具体选项，因此保留为空数组
BRANCHES=("perceptron")

# L2C预取器的选项
L2C_PREFETCHERS=("no" "next_line" "ip_stride" "pangloss" "markov" "ghb")

# LLC预取器的选项
LLC_PREFETCHERS=("no" "next_line" "ip_stride")

# LLC替换策略的选项
LLC_REPLACEMENTS=("bip" "ship" "dip" "red_lfu" "glider" "srrip" "red" "shippp+red" "shippp" "lru" "lfu" "mru" "drrip")

# 遍历所有参数组合
for BRANCH in "${BRANCHES[@]}"; do
	for L2C_PREFETCHER in "${L2C_PREFETCHERS[@]}"; do
		for LLC_PREFETCHER in "${LLC_PREFETCHERS[@]}"; do
			for LLC_REPLACEMENT in "${LLC_REPLACEMENTS[@]}"; do
				# 执行命令
				echo "Executing: ./build_champsim.sh $BRANCH $L1D_PREFETCHER $L2C_PREFETCHER $LLC_PREFETCHER $LLC_REPLACEMENT"
				./build_champsim.sh "$BRANCH" "$L2C_PREFETCHER" "$LLC_PREFETCHER" "$LLC_REPLACEMENT"
			done
		done
	done
done
