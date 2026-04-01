import argparse
import subprocess
import time
import random
import shlex

RANDOM_LIMIT = 1000
SEED = 123456789
random.seed(SEED)

AMMUNITION = [
    'localhost:8080/api/v1/maps/map1',
    'localhost:8080/api/v1/maps'
]

SHOOT_COUNT = 100
COOLDOWN = 0.1


def start_server():
    parser = argparse.ArgumentParser()
    parser.add_argument('server', type=str)
    return parser.parse_args().server


def run(command, output=None):
    process = subprocess.Popen(shlex.split(command), stdout=output, stderr=subprocess.DEVNULL)
    return process


def stop(process, wait=False):
    if process.poll() is None and wait:
        process.wait()
    process.terminate()


def shoot(ammo):
    hit = run('curl ' + ammo, output=subprocess.DEVNULL)
    time.sleep(COOLDOWN)
    stop(hit, wait=True)


def make_shots():
    for _ in range(SHOOT_COUNT):
        ammo_number = random.randrange(RANDOM_LIMIT) % len(AMMUNITION)
        shoot(AMMUNITION[ammo_number])
    print('Shooting complete')

#Запускаем сервере
server = run(start_server())
time.sleep(2)

pid = server.pid
#Запускаем профайлер
pref = run(f'perf record -p {pid} -g -o perf.data', output=subprocess.DEVNULL)
time.sleep(1)
#Отправляем запросы
make_shots()
#Останавливаем профайлер
stop(pref)
time.sleep(1)
#формирование FlameGraph
# perf script | ./FlameGraph/stackcollapse-perf.pl | ./FlameGraph/flamegraph.pl > graph.svg
subprocess.run(f'perf script -i perf.data | ./FlameGraph/stackcollapse-perf.pl | ./FlameGraph/flamegraph.pl > graph.svg', shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
#Останавливаем сервер
stop(server)
time.sleep(1)
print('Job done')
