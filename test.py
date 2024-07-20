import os
import sys
from os.path import isdir
import subprocess
from time import time

from colorama import Fore
from colorama import Style

def remove_file():
    if os.path.isfile('a.exe'):
        os.remove('a.exe')

def remove_newlines(string):
    string = string.replace('\n', '')
    string = string.replace('\r', '')
    return string

def get_absolute_paths(directory):
    paths = list()
    for dirpath,_,filenames in os.walk(directory):
        for f in filenames:
            if f.endswith('.rcp'):
                paths.append(os.path.abspath(os.path.join(dirpath, f)))

    return paths;



os.chdir('tests/output')
files = get_absolute_paths('..')


for file in files:

    command_line = ['../../bin/a.exe', file]
    start_time = time()
    process = subprocess.Popen(command_line, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    process.wait()
    end_time = time()

    if process.stderr is not None and process.stdout is not None:
        subprocess_stderr = str(process.stderr.read().decode('utf-8'))
        subprocess_stdout = str(process.stdout.read().decode('utf-8')) 

        if subprocess_stderr.find('FATAL') != -1 or subprocess_stderr.find('ERROR') != -1:
            print(f'{Fore.RED}[✗]FAIL {Style.RESET_ALL}{file} Time: {end_time - start_time:.2f}\nOutput: {subprocess_stderr}\n')
            remove_file()
            continue

        if subprocess_stdout.find('FATAL') != -1 or subprocess_stdout.find('ERROR') != -1:
            print(f'{Fore.RED}[✗]FAIL {Style.RESET_ALL}{file} Time: {end_time - start_time:.2f}\nOutput: {subprocess_stderr}\n')
            remove_file()
            continue

        if len(subprocess_stdout) == 0:
            print(f'{Fore.RED}[✗]FAIL {Style.RESET_ALL}{file} CRASHED')
            remove_file()
            continue

    if not os.path.isfile('a.exe'):
        continue

        # Run the compiled program
    command_line = ['a.exe']
    process = subprocess.Popen(command_line)
    process.wait()

    if process.returncode == 0:
        print(f'{Fore.GREEN}[✓]OK {Style.RESET_ALL}{file} Time: {end_time - start_time:.2f}')
    else:
        print(f'{Fore.RED}[✗]FAIL {Style.RESET_ALL}{file} Got: {process.returncode}')

    remove_file()








