import os
import sys
from os.path import isdir
import subprocess
from time import time

from colorama import Fore
from colorama import Style

def right_pad(str, len_s):
    while len(str) < len_s:
        str += ' '
    return str

def remove_file():
    if os.path.isfile('a.exe'):
        os.remove('a.exe')

def remove_newlines(string):
    string = string.replace('\n', '')
    string = string.replace('\r', '')
    return string

def get_tests():
    paths = ['arrays', 'basic', 'fn_call', 'fn_ptr', 'pointers', 'struct', 'pass_struct', 'struct_in_struct', 'lambda']
    return paths


tests = get_tests()
os.chdir('tests')
for test in tests:
    os.chdir(test)

    c_start_time = time()
    command_line = ['rcp', 'build.rcp']
    process = subprocess.Popen(command_line, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = process.communicate()
    c_end_time = time()

    if not os.path.isfile('a.exe'):
        print(f'{Fore.RED}[✗]FAIL {Style.RESET_ALL}{test} Time: {c_end_time - c_start_time:.2f}s')
        print(f'Output: {stderr.decode()}\n{stdout.decode()}')

    else:
        # Run the compiled program
        start_time = time()
        command_line = ['a.exe']
        process = subprocess.Popen(command_line)
        process.wait()
        end_time = time()
        if process.returncode == 0:
            ok_str = f'{Fore.GREEN}[✓]OK {Style.RESET_ALL}{test}'
            ok_str = right_pad(ok_str, 35)
            print(f'{ok_str}Compile: {c_end_time - c_start_time:.2f}s Execute: {end_time - start_time:.2f}s')
        else:
            print(f'{Fore.RED}[✗]FAIL {Style.RESET_ALL}{test} Got: {process.returncode}')
    remove_file()

    os.chdir('..')








