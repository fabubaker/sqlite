import sys
import random
import subprocess

def generate_sql(num_ops):
    sql = 'SELECT '

    operator = ''
    for i in range(0, num_ops):
        operand_type = random.choice(['Int', 'Real'])

        if operand_type == 'Int':
            operand = random.randint(1, 2147483647)
        else:
            operand = random.uniform(1, 2147483647)

        sql += operator + str(operand)
        operator = random.choice(['+', '-', '*'])

    return sql

SQLITE_JIT = './sqlite3-jit'
SQLITE_NO_JIT = './sqlite3-no-jit'

USAGE = 'python3 run-842-benchmarks.py <# of operands>'

if len(sys.argv) != 2:
    print('Incorrect # of params.')
    print('USAGE: ' + USAGE)
    print('Requires python >= 3.5.')
    exit(1)

sqlite_jit_proc = lambda input: subprocess.run([SQLITE_JIT], input=input.encode('utf-8'), stdout=subprocess.PIPE, check=True)
sqlite_no_jit_proc = lambda input: subprocess.run([SQLITE_NO_JIT], input=input.encode('utf-8'), stdout=subprocess.PIPE, check=True)

num_operands = int(sys.argv[1])
sql = generate_sql(num_operands)
print("Executing the following SQL with %d operands:" % num_operands)
print(sql)
print()

sqlite_jit_p = sqlite_jit_proc(sql)
sqlit_jit_output = sqlite_jit_p.stdout.decode('utf-8').split('\n')
print("=== JIT benchmarks ===")
total = 0.0
for stmt in sqlit_jit_output:
    if stmt.startswith('<Benchmarks>'):
        print(stmt)

    if 'execution' in stmt:
        total += float(stmt.split(' ')[-2])
print("Total execution time: %f" % total)

sqlite_no_jit_p = sqlite_no_jit_proc(sql)
sqlite_no_jit_output = sqlite_no_jit_p.stdout.decode('utf-8').split('\n')
print("=== non-JIT benchmarks ===")
for stmt in sqlite_no_jit_output:
    if stmt.startswith('<Benchmarks>'):
        print(stmt)

    if 'execution' in stmt:
        total += float(stmt.split(' ')[-2])
print("Total execution time: %f" % total)