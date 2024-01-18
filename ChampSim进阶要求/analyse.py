# Let's parse the allresult.txt file and calculate the averages for each combination.
# We'll store the results in a dictionary and then write them to result.txt.

import re

def parse_results(file_path):
    with open(file_path, 'r') as file:
        lines = file.readlines()

    # Define patterns to extract the necessary information
    file_pattern = r"perceptron-(\S+)-(\S+)-(\S+)-(\S+)-1core_(\S+)\.txt"
    ipc_pattern = r"Core_0_IPC:\s*([\d.]+)"
    branch_accuracy_pattern = r"Core_0_branch_prediction_accuracy:\s*([\d.]+)"
    l1d_hit_rate_pattern = r"Core_0_L1D Hit Rate:\s*Hit Rate:\s*([\d.]+)"
    l2c_hit_rate_pattern = r"Core_0_L2C Hit Rate:\s*Hit Rate:\s*([\d.]+)"
    llc_hit_rate_pattern = r"Core_0_LLC Hit Rate:\s*Hit Rate:\s*([\d.]+)"

    # Initialize the data structure to hold the results
    results = {}

    current_file = None
    for line in lines:
        # Check if we're processing a new file
        file_match = re.search(file_pattern, line)
        if file_match:
            current_file = '-'.join(file_match.groups()[:-1])
            if current_file not in results:
                results[current_file] = {
                    'IPC': [],
                    'branch_prediction_accuracy': [],
                    'L1D Hit Rate': [],
                    'L2C Hit Rate': [],
                    'LLC Hit Rate': []
                }
            continue

        # If we're processing results for a file, collect the metrics
        if current_file:
            for metric, pattern in [('IPC', ipc_pattern), 
                                    ('branch_prediction_accuracy', branch_accuracy_pattern), 
                                    ('L1D Hit Rate', l1d_hit_rate_pattern), 
                                    ('L2C Hit Rate', l2c_hit_rate_pattern), 
                                    ('LLC Hit Rate', llc_hit_rate_pattern)]:
                
                match = re.search(pattern, line)
                if match:
                    value = match.group(1)
                    if value != 'N/A':
                        results[current_file][metric].append(float(value))

    return results

def calculate_averages(results):
    averages = {}
    for strategy, metrics in results.items():
        averages[strategy] = {}
        for metric, values in metrics.items():
            if values:
                averages[strategy][metric] = sum(values) / len(values)
            else:
                averages[strategy][metric] = 'N/A'
    return averages

def save_results_to_file(averages, file_path):
    with open(file_path, 'w') as file:
        for strategy, metrics in averages.items():
            file.write(f"{strategy}:\n")
            for metric, average in metrics.items():
                file.write(f"{metric}: {average}")
                if 'Hit Rate' in metric and average != 'N/A':
                    file.write(f"%\n")
                else:
                    file.write(f"\n")
            file.write("--------------------------------\n")

results = parse_results('processed_result.txt')
averages = calculate_averages(results)
save_results_to_file(averages, 'result.txt')

