import re
import os
from collections import defaultdict

def parse_text_file(file_path):
    counter_dict = {}

    with open(file_path, 'r') as file:
        lines = file.readlines()

    summary_pattern = r"instructions: (\d+) cycles: (\d+) cumulative IPC: ([\d.]+) \(Simulation time: (\d+) hr (\d+) min (\d+) sec\)"
    branch_pattern = r"^CPU \d+ Branch Prediction Accuracy: (\d+\.?\d*)% MPKI: (\d+\.?\d*)"
    LLC_pattern = r"^LLC TOTAL\s+ACCESS:\s+(\d+)\s+HIT:\s+(\d+)\s+MISS:\s+(\d+)"

    for line in lines:
        if "Simulation complete" in line:
            match = re.search(summary_pattern, line)
            instructions = int(match.group(1))
            cycles = int(match.group(2))
            ipc = float(match.group(3))
            hours = int(match.group(4))
            minutes = int(match.group(5))
            seconds = int(match.group(6))

            counter_dict["instructions"] = instructions
            counter_dict["cycles"] = cycles
            counter_dict["ipc"] = ipc
            counter_dict["total_time"] = hours*60*60 + minutes*60 + seconds
        
        if "Branch Prediction Accuracy" in line:
            match = re.search(branch_pattern, line)
            branch_pred_acc = float(match.group(1))
            mpki = float(match.group(2))

            counter_dict["branch_prediction_accuracy"] = branch_pred_acc
            counter_dict["bMPKI"] = mpki

        if "LLC TOTAL" in line:
            match = re.search(LLC_pattern, line)
            llc_access = int(match.group(1))
            llc_hit = int(match.group(2))
            llc_miss = int(match.group(3))

            counter_dict["llc_access"] = llc_access
            counter_dict["llc_hit"] = llc_hit
            counter_dict["llc_miss"] = llc_miss

    return counter_dict

def process_directory(directory_path):
    result_dict = defaultdict(lambda: defaultdict(list))

    for root, dirs, files in os.walk(directory_path):
        for filename in files:
            file_path = os.path.join(root, filename)

            # Call the function to get the dictionary
            counter_dict = parse_text_file(file_path)

            file_split = filename.split(".")
            rm_jobid_filename = ""
            for i in range(len(file_split)-1):
                rm_jobid_filename+= str(file_split[i])+"."
            rm_jobid_filename = rm_jobid_filename[:-1]


            for k, v in counter_dict.items():
                result_dict[rm_jobid_filename][k].append(v)

    return result_dict