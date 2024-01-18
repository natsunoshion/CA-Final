import re
from tabulate import tabulate

def read_results(file_path):
    with open(file_path, 'r') as file:
        content = file.read()
    
    # Split the content by the strategy blocks
    strategy_blocks = content.strip().split('--------------------------------\n')

    # Define pattern to extract the necessary information
    strategy_pattern = r"(next_line-(\S+)-(\S+)-(\S+)):"
    ipc_pattern = r"IPC: ([\d.]+)"

    results = {}
    for block in strategy_blocks:
        strategy_match = re.search(strategy_pattern, block)
        ipc_match = re.search(ipc_pattern, block)
        if strategy_match and ipc_match:
            strategy = strategy_match.group(1)
            l2c_prefetch = strategy_match.group(2)
            llc_prefetch = strategy_match.group(3)
            llc_replacement = strategy_match.group(4)
            ipc = float(ipc_match.group(1))

            # Filter for specific L2C prefetch strategies and LLC replacement strategies
            if llc_replacement in ['drip', 'lru', 'ship', 'srrip', 'shippp'] and l2c_prefetch in ['no', 'next_line', 'ip_stride', 'pangloss']:
                # Organize the results by the L2C Prefetch, LLC Prefetch, LLC Replacement
                if l2c_prefetch not in results:
                    results[l2c_prefetch] = {}
                if llc_prefetch not in results[l2c_prefetch]:
                    results[l2c_prefetch][llc_prefetch] = {}
                if llc_replacement not in results[l2c_prefetch][llc_prefetch]:
                    results[l2c_prefetch][llc_prefetch][llc_replacement] = []

                results[l2c_prefetch][llc_prefetch][llc_replacement].append(ipc)

    return results

def format_results_as_table(results):
    # Prepare data for tabulation
    rows = []
    for l2c_prefetch, llc_prefetches in results.items():
        for llc_prefetch, llc_replacements in llc_prefetches.items():
            for llc_replacement, ipcs in llc_replacements.items():
                average_ipc = sum(ipcs) / len(ipcs) if ipcs else 'N/A'
                rows.append([l2c_prefetch, llc_prefetch, llc_replacement, average_ipc])
    
    # Sort rows by the Average IPC in descending order, then by L2C Prefetch, l2c Prefetch, l2c Replacement
    rows = sorted(rows, key=lambda x: (x[0], x[1], x[2]))
    # Use tabulate to create a table
    headers = ['L2C Prefetch', 'LLC Prefetch', 'LLC Replacement', 'Average IPC']
    table = tabulate(rows, headers, tablefmt='grid')
    return table

# Read and process the results
results = read_results('result.txt')
table = format_results_as_table(results)
print(table)

