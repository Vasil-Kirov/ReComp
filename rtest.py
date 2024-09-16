import os
import sys
from os.path import isdir
from sys import platform
import subprocess
from time import time

from colorama import Fore
from colorama import Style

def right_pad(str, len_s):
    while len(str) < len_s:
        str += ' '
    return str

def remove_file(path):
    if os.path.isfile(path):
        os.remove(path)

def remove_newlines(string):
    string = string.replace('\n', '')
    string = string.replace('\r', '')
    return string

def get_tests():
    paths = ['arrays', 'basic', 'fn_call', 'fn_ptr', 'pointers', 'struct', 'pass_struct', 'struct_in_struct', 'lambda', 'pass_complex', 'iterators', 'slices', 'var_args', 'loop_and_if', 'generics', 'defer', 'if_else', 'dynamic_array']
    paths.sort()
    return paths

dir_path = os.path.dirname(os.path.realpath(__file__))
tests = get_tests()
os.chdir('tests')
for test in tests:
    os.chdir(test)

    exe_name = ''
    if platform == 'linux':
        exe_name = 'a'
    elif platform == 'win32':
        exe_name = 'a.exe'
    else:
        exit(1)

    c_start_time = time()
    command_line = []


    if platform == 'linux':
        command_line = [dir_path + '/bin/rcp', 'build.rcp']
    elif platform == 'win32':
        command_line = [dir_path + '/bin/rcp.exe', 'build.rcp']
    else:
        exit(1)

    process = subprocess.Popen(command_line, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = process.communicate()
    c_end_time = time()

    full_path = dir_path + '/tests/' + test + '/' + exe_name
    if not os.path.isfile(full_path):
        print(f'{Fore.RED}[✗]FAIL {Style.RESET_ALL}{test} Time: {c_end_time - c_start_time:.2f}s')
        print(f'Output: {stderr.decode()}\n{stdout.decode()}')
    else:
        # Run the compiled program
        start_time = time()
        command_line = [full_path]
        process = subprocess.Popen(command_line)
        process.wait()
        end_time = time()
        if process.returncode == 0:
            ok_str = f'{Fore.GREEN}[✓]OK {Style.RESET_ALL}{test}'
            ok_str = right_pad(ok_str, 35)
            print(f'{ok_str}Compile: {c_end_time - c_start_time:.2f}s Execute: {end_time - start_time:.2f}s')
        else:
            print(f'{Fore.RED}[✗]FAIL {Style.RESET_ALL}{test} Got: {process.returncode}')
    remove_file(full_path)

    os.chdir('..')








